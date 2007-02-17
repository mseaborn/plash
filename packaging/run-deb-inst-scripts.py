#!/usr/bin/python

"""Run preinst and postinst scripts for Debian packages."""

import os
import sys

import plash.namespace as ns
import plash.mainloop
import plash.process

import plash_pkg


class File_namespace:

    def __init__(self):
        self._root = plash.env.get_root_dir()
        self._cwd = ns.resolve_dir(self._root, os.getcwd())

    def get_obj(self, pathname):
        return ns.resolve_obj(self._root, pathname,
                              cwd=self._cwd, nofollow=False)


unpack_cache = os.path.realpath("unpack-cache")

cwd = os.getcwd()
my_root = File_namespace()
os.chdir(cwd)


def init_package(app_dir, package, control_script, args):
    proc = plash.process.Process_spec_ns()
    proc.cwd_path = "/"

    root_dir = ns.make_cow_dir(my_root.get_obj(
                                 os.path.join(app_dir, "unpacked")),
                               my_root.get_obj(
                                 os.path.join(app_dir, "write_layer")))
    ns.attach_at_path(proc.root_node, "/", root_dir)
    ns.resolve_populate(my_root._root, proc.root_node,
                        "/usr/lib/plash/lib", my_root._cwd)

    script_path = os.path.join(unpack_cache, package,
                               "control", control_script)

    if os.path.exists(script_path):
        ns.attach_at_path(proc.root_node, "/script",
                          my_root.get_obj(script_path))
        proc.cmd = "/script"
        proc.args = args
        pid = proc.spawn()
        plash.mainloop.run_server()
    else:
        print "  no %s" % control_script

def init_packages(app_dir):
    fh = open(os.path.join(app_dir, "package-list"), "r")
    try:
        for block in plash_pkg.file_blocks(fh):
            info = plash_pkg.block_fields(block)
            package = "%s_%s" % (info["package"], info["version"])
            print package
            init_package(app_dir, package, "preinst", ["install"])
            init_package(app_dir, package, "postinst", ["configure"])
    finally:
        fh.close()

def main(args):
    if len(args) != 1:
        print "Usage: %s <app-dir>" % sys.argv[0]
    else:
        init_packages(args[0])


if __name__ == "__main__":
    main(sys.argv[1:])
