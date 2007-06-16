# Copyright (C) 2006, 2007 Mark Seaborn
#
# This file is part of Plash.
#
# Plash is free software; you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License as
# published by the Free Software Foundation; either version 2.1 of
# the License, or (at your option) any later version.
#
# Plash is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with Plash; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301,
# USA.

import gtk
import plash.marshal
import plash.namespace
import traceback


class Powerbox(plash.marshal.Pyobj_demarshal):

    def __init__(self, user_namespace, app_namespace, pet_name):
        self.dir = '/'
        self.user_namespace = user_namespace
        self.app_namespace = app_namespace
        self.pet_name = pet_name

    # Dispatch incoming invocations to cap_call_cont
    def cap_invoke(self, args):
        try:
            (method, args2) = plash.marshal.method_unpack(args)
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
        x = plash.marshal.methods_by_name['powerbox_req_filename']['packer'].unpack_r(args)
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
                transient_for = arg[1]
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
                self.app_namespace.resolve_populate(self.user_namespace,
                                                    pathname)
                cont.cap_invoke(plash.marshal.pack('powerbox_result_filename',
                                                   pathname))
            else:
                cont.cap_invoke(plash.marshal.pack('fail', 0))
            dialog.hide()

        dialog.connect('response', response)

        if transient_for is not None:
            dialog.realize()
            dialog.window.property_change(
                gtk.gdk.atom_intern("WM_TRANSIENT_FOR", False),
                gtk.gdk.atom_intern("WINDOW", False),
                32,
                gtk.gdk.PROP_MODE_REPLACE,
                [transient_for])
        
        dialog.show()
