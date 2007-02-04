#!/usr/bin/python

# Usage:  launch-pet-app --app-dir <app-dir> [--open-files [<file> ...]]


import os
import sys
import traceback
import plash_core
import plash.env
import plash.mainloop
import plash.namespace as ns
from plash.process import Process_spec
import plash.pola_run_args as pola_run_args
import plash.powerbox

import pkg


def read_config(filename):
    file = open(filename, "r")
    try:
        return pkg.block_fields(pkg.get_block(file))
    finally:
        file.close()

def main(args):
    app_dir = None
    files = []

    while len(args) > 0:
        if args[0] == '--app-dir':
            app_dir = args[1]
            args = args[2:]
        elif args[0] == '--open-file':
            files.append(args[1])
            args = args[2:]
        elif args[0] == '--open-files':
            files.extend(args[1:])
            args = []
        else:
            break

    try:
        launch_app(app_dir, files, args)
    except:
        f = open(os.path.join(app_dir, "launch_log"), "a")
        f.write("--------\n")
        f.write("Got exception:\n" + traceback.format_exc())
        f.close()

def launch_app(app_dir, files, args):
    config = read_config(os.path.join(app_dir, "config"))

    caller_root = plash.env.get_root_dir()
    proc = Process_spec()
    proc.env = os.environ.copy()
    proc.root_node = ns.make_node()
    proc.caps = { 'conn_maker': ns.conn_maker }
    proc.cmd = None
    proc.arg0 = "-"
    proc.args = []

    class State:
        pass
    state = State()
    state.namespace_empty = True
    state.caller_root = caller_root
    state.cwd = ns.resolve_dir(state.caller_root, os.getcwd())
    state.pet_name = None
    state.powerbox = False

    pola_run_args.handle_args(state, proc, args)
    
    pola_run_args.handle_args(state, proc, \
                              ["-fw", "/dev/null",
                               "-f", "/dev/urandom",
                               "-f", "/dev/random",
                               "-f", "/usr/lib/plash/lib",
                               "--x11",
                               "--net"])

    if proc.cmd == None:
        # We don't implement looking up in PATH yet, so get shell to do it
        proc.cmd = "/bin/sh"
        proc.arg0 = proc.cmd
        proc.args = ["-c", config["exec"], "-"]

    pola_run_args.set_fake_uids(proc)

    dir = ns.resolve_obj(caller_root, app_dir)

    dir_unpacked = dir.dir_traverse("unpacked")
    dir_write_layer = dir.dir_traverse("write_layer")
    root_dir = ns.make_cow_dir(dir_write_layer, dir_unpacked)
    ns.attach_at_path(proc.root_node, "/", root_dir)

    # TODO: grant read-only access to apps configured as viewers
    for filename in files:
        ns.resolve_populate(state.caller_root, proc.root_node,
                            filename,
                            flags=ns.FS_FOLLOW_SYMLINKS | ns.FS_SLOT_RWC)
        proc.args.append(filename)

    fs_op = plash_core.make_fs_op(ns.dir_of_node(proc.root_node))
    fs_op.fsop_chdir("/")
    proc.caps["fs_op"] = fs_op

    proc.caps["powerbox_req_filename"] = \
        plash.powerbox.Powerbox(user_namespace = state.caller_root,
                                app_namespace = proc.root_node,
                                pet_name = config["pet-name"])
    plash.mainloop.use_gtk_mainloop()

    pid = proc.spawn()
    plash.mainloop.run_server()
    # No need to wait for subprocess to exit.


if __name__ == "__main__":
    main(sys.argv[1:])
