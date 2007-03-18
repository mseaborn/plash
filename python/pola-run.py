#!/usr/bin/python

import sys
import os
import plash.env
import plash.mainloop
import plash.namespace as ns
import plash.pola_run_args
from plash.pola_run_args import BadArgException
import plash.process


def usage():
    print """
Usage: pola-run [options]
  --prog <file>   Gives filename of executable to invoke
  [ -f[awls]... <path>
                  Grant access to given file or directory
  | -t[awls]... <path-dest> <path-src>
                  Attach file/dir <path-src> into namespace at <path-dest>
  | -a <string>   Add string to argument list
  | --cwd <dir>   Set the current working directory (cwd)
  | --no-cwd      Set the cwd to be undefined
  | --copy-cwd    Copy cwd from calling process
  ]...
  [-B]            Grant access to /usr, /bin and /lib
  [--x11]         Grant access to X11 Window System
  [--net]         Grant access to network config files
  [--log]         Print method calls client makes to file server
  [--pet-name <name>]
  [--powerbox]
  [-e <command> <arg>...]
"""

def main(args):
    proc = plash.process.Process_spec_ns()
    proc.env = os.environ.copy()

    state = plash.pola_run_args.ProcessSetup(proc)
    state.namespace_empty = True
    state.caller_root = plash.env.get_root_dir()
    state.cwd = ns.resolve_dir(state.caller_root, os.getcwd())
    state.pet_name = None
    state.powerbox = False

    try:
        state.handle_args(args)
        if proc.cmd == None:
            raise BadArgException, "No command name given (use --prog or -e)"
    except BadArgException, msg:
        print "pola-run: error:", msg
        sys.exit(1)

    if state.namespace_empty:
        print "pola-run: warning: namespace empty, try -B for default namespace"

    plash.pola_run_args.set_fake_uids(proc)

    if state.cwd != None:
        proc.cwd_path = ns.dirstack_get_path(state.cwd)

    if state.powerbox:
        powerbox = __import__("plash.powerbox")
        # The pet name defaults to the executable name
        if state.pet_name == None:
            state.pet_name = proc.cmd
        proc.caps['powerbox_req_filename'] = \
            powerbox.Powerbox(user_namespace = state.caller_root,
                              app_namespace = proc.root_node,
                              pet_name = state.pet_name)
        plash.mainloop.use_gtk_mainloop()

    fwd_stdout, fwd_stderr = plash.pola_run_args.proxy_terminal(proc)
    pid = proc.spawn()
    plash.mainloop.run_server()
    fwd_stdout.flush()
    fwd_stderr.flush()

    # Wait for the subprocess to exit and check the exit code.
    (pid2, status) = os.waitpid(pid, 0)
    assert pid == pid2
    if os.WIFEXITED(status):
        sys.exit(os.WEXITSTATUS(status))
    else:
        # FIXME
        sys.exit(1)


if __name__ == "__main__":
    main(sys.argv[1:])
