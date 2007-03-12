
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


def make_process(app_dir):
    my_root = FileNamespace()
    caller_root = plash.env.get_root_dir()
    proc = plash.process.Process_spec_ns()
    proc.env = os.environ.copy()
    proc.cwd_path = "/"

    class State:
        pass
    state = State()
    state.namespace_empty = True
    state.caller_root = caller_root
    state.cwd = ns.resolve_dir(state.caller_root, os.getcwd())
    state.pet_name = None
    state.powerbox = False
    
    plash.pola_run_args.handle_args(
        state, proc,
        ["-fw", "/dev/null",
         "-f", "/dev/urandom",
         "-f", "/dev/random",
         "-f", "/usr/lib/plash/lib",
         "-f", "/etc/localtime",
         "--x11",
         "--net"])

    root_dir = ns.make_cow_dir(my_root.get_obj(
                                 os.path.join(app_dir, "unpacked")),
                               my_root.get_obj(
                                 os.path.join(app_dir, "write_layer")))
    ns.attach_at_path(proc.root_node, "/", root_dir)

    return (proc, state)
