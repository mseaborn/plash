
; Emacs commands can get filenames from the user with declarations such as:
; (interactive "fPrompt: ")
; This is turned into a call to the function read-file-name
; (which is defined in minibuf.el)
; We replace read-file-name.

; Note that XEmacs' usual read-file-name calls a function,
; should-use-dialog-box-p, to determine whether to open a dialog box
; or prompt using the minibuffer.
; We always use a dialog box.



;(sefvar powerbox-process nil
;	"The current process for handling a Plash powerbox request.")

;(defun read-file-name (prompt
;                       &optional dir default must-match initial-contents
;		       history)
;  (let ((process-connection-type nil)) ; Use a pipe
;    (setq powerbox-process
;	  (start-process "plash-powerbox-req" "*scratch*" "/bin/echo" "/home/mrs/foo"))
;    (set-process-filter powerbox-process 'powerbox-process-filter)
;    ; If Emacs exits, don't ask about killing the process
;    (process-kill-without-query plash-process)))



; NB. XEmacs doesn't redraw while waiting for result

(defun powerbox-read-file-name-sync
  (prompt
   &optional dir default must-match initial-contents
   history)
  (let* ((buffer (generate-new-buffer "*powerbox-request*"))
	 (status
	  (call-process "/usr/bin/powerbox-req" nil buffer t
			"--save"
			"--desc"
			(format "Requesting a file using the prompt \"%s\""
				prompt)
			"--dir"
			(expand-file-name
			 (if (null dir)
			     (default-directory)
			   dir))))
	 (name (buffer-string buffer)))
    (kill-buffer buffer)
    (if (equal status 0)
	; Remove trailing newline if present
	(if (and (> (length name) 0)
		 (= (elt name (- (length name) 1)) ?\n))
	    (substring name 0 (- (length name) 1))
	  name)
      (error (format "File open dialogue box cancelled: %s" name)))))

(defun powerbox-sentinel (a b) nil)

(defun powerbox-read-file-name
  (prompt
   &optional dir default must-match initial-contents
   history)
  (let* ((buffer (generate-new-buffer "*powerbox-request*"))
	 (process-connection-type nil) ; Use a pipe
	 (process
	  (start-process "*powerbox-request*" buffer
			 "/usr/bin/powerbox-req"
			 "--save"
			 "--desc"
			 (format "Requesting a file using the prompt \"%s\""
				 prompt)
			 "--dir"
			 (expand-file-name
			  (if (null dir)
			      (default-directory)
			    dir)))))
    ; Setting the sentinel to nil doesn't seem to switch off XEmacs'
    ; default sentinel as the documentation says it should, so I have
    ; defined a dummy one.
    (set-process-sentinel process 'powerbox-sentinel)
    ; Wait until process has exited
    ; Using accept-process-output allows some of XEmacs' redrawing of the
    ; display to happen, such as menus and scrollbars, but not buffer
    ; contents and modelines.  It makes XEmacs do a busy wait, which is
    ; bad.
    ; Using next-event and dispatch-event, editing can continue as normal.
    ; This is *not* modal.  It's like using the minibuffer normally.
    ; Note however that our event loop is on the stack, and XEmacs only
    ; has one stack!
    ;(while (equal 'run (process-status process))
    ;  (accept-process-output))
    (while (equal 'run (process-status process))
      (dispatch-event (next-event)))
    (let ((name (buffer-string buffer)))
      (kill-buffer buffer)
      (if (equal (process-exit-status process) 0)
	  ; Remove trailing newline if present
	  (if (and (> (length name) 0)
		   (= (elt name (- (length name) 1)) ?\n))
	      (substring name 0 (- (length name) 1))
	    name)
	(error (format "File open dialogue box cancelled: %s" name))))))


; Before replacing the definition of read-file-name, save the original one.
(if (not (boundp 'minibuffer-read-file-name))
    (fset 'minibuffer-read-file-name
	  (symbol-function 'read-file-name)))


(defvar powerbox-enabled t
  "When t, read-file-name will use a powerbox.
When nil, read-file-name will use the minibuffer to get a filename.")

(defun read-file-name (&rest args)
  (if powerbox-enabled
      (apply 'powerbox-read-file-name args)
      (apply 'minibuffer-read-file-name args)))
