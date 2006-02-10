; Copyright (C) 2005 Mark Seaborn
;
; This file is part of Plash, the Principle of Least Authority Shell.
;
; Plash is free software; you can redistribute it and/or modify it
; under the terms of the GNU Lesser General Public License as
; published by the Free Software Foundation; either version 2.1 of
; the License, or (at your option) any later version.
;
; Plash is distributed in the hope that it will be useful, but
; WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
; Lesser General Public License for more details.
;
; You should have received a copy of the GNU Lesser General Public
; License along with Plash; if not, write to the Free Software
; Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
; USA.

(load-library "gnuserv")

(defvar plash-process nil
  "The current plash-gnuserv process.")

(defvar plash-string ""
  "The last input string from the server.")

(defun plash-process-filter (proc string)
  "Process plash-gnuserv client requests to execute Emacs commands."
  (setq plash-string (concat plash-string string))
  ;; C-d means end of request.
  (let ((index (string-match "\C-d" plash-string)))
     (if (numberp index)
         (let ((req (substring plash-string 0 index))
               (rest (substring plash-string (+ index 1))))
            (setq plash-string rest)
            (gnuserv-edit-files '(x ":0") (list (cons 1 req)))))))

(defun plash-gnuserv-start ()
  (interactive)
  (let ((process-connection-type nil)) ; Use a pipe
    (setq plash-process
       (start-process "plash-gnuserv" "*scratch*" "/bin/plash-gnuserv" "--serv"))
    (set-process-filter plash-process 'plash-process-filter)
    ; When Emacs exits, don't ask about killing the process
    (process-kill-without-query plash-process)))
