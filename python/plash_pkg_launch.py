
import os

import plash.env
import plash.namespace as ns
import plash.pola_run_args
import plash.process


def make_process(app_dir):
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
         "--x11",
         "--net"])

    return (proc, state)
