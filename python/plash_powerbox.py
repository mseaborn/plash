
import gtk
import plash_marshal
import plash_namespace
import traceback


class Powerbox(plash_marshal.Pyobj_demarshal):

    def __init__(self, user_namespace, app_namespace, pet_name):
        self.dir = '/'
        self.user_namespace = user_namespace
        self.app_namespace = app_namespace
        self.pet_name = pet_name

    # Dispatch incoming invocations to cap_call_cont
    def cap_invoke(self, args):
        try:
            (method, args2) = plash_marshal.method_unpack(args)
            if method == 'Call':
                (data, caps, fds) = args2
                self.cap_call_cont((data, caps[1:], fds), caps[0])
            else:
                # There is nothing we can do, besides warning
                print "Powerbox.cap_invoke: no match"
        except:
            traceback.print_exc()

    # Dispatch to powerbox_req_filename_cont
    def cap_call_cont(self, args, cont):
        x = plash_marshal.methods_by_name['powerbox_req_filename']['packer'].unpack_r(args)
        self.powerbox_req_filename_cont(x, cont)
    
    def powerbox_req_filename_cont(self, args, cont):
        
        save_mode = False
        want_dir = False
        start_dir = '' # not used yet
        transient_for = None # parent window ID

        for arg in args:
            if arg[0] == 'Startdir':
                start_dir = arg[1]
            elif arg[0] == 'Save':
                save_mode = True
            elif arg[0] == 'Wantdir':
                want_dir = True
            elif arg[0] == 'Transientfor':
                transient_dir = arg[1]
            else:
                print "powerbox: unknown arg:", arg
        
        dialog = gtk.FileChooserDialog()

        if save_mode:
            main_action_text = gtk.STOCK_SAVE
        else:
            main_action_text = gtk.STOCK_OPEN
        dialog.add_button(gtk.STOCK_CLOSE, gtk.RESPONSE_CANCEL)
        dialog.add_button(main_action_text, gtk.RESPONSE_ACCEPT)
        
        tips = gtk.Tooltips()
        tips.set_tip(dialog.action_area.get_children()[0],
                     "Grant <%s> the ability to access the file" % self.pet_name)

        if want_dir:
            if save_mode:
                title = "Create and select directory"
                dialog.set_action(gtk.FILE_CHOOSER_ACTION_CREATE_FOLDER)
            else:
                title = "Select directory"
                dialog.set_action(gtk.FILE_CHOOSER_ACTION_SELECT_FOLDER)
        else:
            if save_mode:
                title = "Save file"
                dialog.set_action(gtk.FILE_CHOOSER_ACTION_SAVE)
            else:
                title = "Open file"
                dialog.set_action(gtk.FILE_CHOOSER_ACTION_OPEN)

        dialog.set_title("<%s>: %s" % (self.pet_name, title))
        dialog.set_current_folder(self.dir)

        read_only = gtk.CheckButton(label="Grant only read access")
        read_only.show()
        dialog.set_extra_widget(read_only)
        
        def response(widget, response_id):
            if response_id == gtk.RESPONSE_ACCEPT:
                pathname = dialog.get_filename()
                print "response =", response_id, pathname
                plash_namespace.resolve_populate(self.user_namespace,
                                                 self.app_namespace,
                                                 pathname)
                cont.cap_invoke(plash_marshal.pack('powerbox_result_filename',
                                                   pathname))
            dialog.hide()

        dialog.connect('response', response)
        
        dialog.show()
