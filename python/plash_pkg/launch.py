
import os

import plash.env
import plash.namespace as ns
import plash.pola_run_args
import plash.process


class FileNamespace:

    def __init__(self):
        self._root = plash.env.get_root_dir()
        self._cwd = ns.resolve_dir(self._root, os.getcwd())

    def get_obj(self, pathname):
        return ns.resolve_obj(self._root, pathname,
                              cwd=self._cwd, nofollow=False)


class Process(plash.process.Process_spec_ns):

    def _set_up_library_path(self):
        # Don't set LD_LIBRARY_PATH
        pass


def make_process(app_dir):
    my_root = FileNamespace()
    caller_root = plash.env.get_root_dir()
    proc = Process()
    proc.env = os.environ.copy()
    proc.cwd_path = "/"

    state = plash.pola_run_args.ProcessSetup(proc)
    state.caller_root = caller_root
    state.cwd = ns.resolve_dir(state.caller_root, os.getcwd())
    
    state.handle_args(
        ["-fw", "/dev/null",
         "-f", "/dev/urandom",
         "-f", "/dev/random",
         "-f", "/etc/localtime",
         "--x11",
         "--net"])

    lib_dir_path = os.environ.get("PLASH_LIBRARY_DIR", "/usr/lib/plash/lib")
    lib_dir = my_root.get_obj(lib_dir_path)
    for dir_entry in lib_dir.dir_list():
        leafname = dir_entry["name"]
        file_obj = lib_dir.dir_traverse(leafname)
        ns.attach_at_path(proc.root_node, os.path.join("/lib", leafname),
                          file_obj)

    root_dir = ns.make_cow_dir(my_root.get_obj(
                                 os.path.join(app_dir, "write_layer")),
                               my_root.get_obj(
                                 os.path.join(app_dir, "unpacked")))
    ns.attach_at_path(proc.root_node, "/", root_dir)

    return (proc, state)
