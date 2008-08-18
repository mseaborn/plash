
# Copyright (C) 2007 Mark Seaborn
#
# chroot_build is free software; you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as
# published by the Free Software Foundation; either version 2.1 of the
# License, or (at your option) any later version.
#
# chroot_build is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with chroot_build; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
# 02110-1301, USA.

"""
Usage: in_chroot [<command> <args>...]

Runs a command in a chroot.  The chroot directory to use is inferred
from the current working directory, and the working directory is
preserved for the command.

For example:
$ export CHROOTS_DIR=~/.../chroots
$ cd $CHROOTS_DIR/feisty/chroot/some/dir
$ in_chroot COMMAND
COMMAND sees the current directory as "/some/dir".
"""

import os
import sys


def remove_prefix(prefix, string):
    assert string.startswith(prefix)
    return string[len(prefix):]


def in_dir(dir_path, args):
    return ["sh", "-c", 'cd "$1" && shift && exec "$@"',
            "x", dir_path] + args


class Error(Exception):

    pass


def get_chroot():
    if "CHROOTS_DIR" not in os.environ:
        raise Error("Set CHROOTS_DIR to the directory containing "
                    "chroot directories")
    chroots_dir = os.environ["CHROOTS_DIR"]
    cwd = os.getcwd()
    chroots = [os.path.realpath(os.path.join(
                   chroots_dir, leafname, "chroot"))
               for leafname in os.listdir(chroots_dir)]
    for chroot_dir in chroots:
        if cwd == chroot_dir:
            return (chroot_dir, ".")
        elif cwd.startswith(chroot_dir + "/"):
            return (chroot_dir, remove_prefix(chroot_dir, cwd).lstrip("/"))
    raise Error("cwd not in a defined chroot: %s" % cwd)


def main(args):
    if args[0] != "--user":
        preserve_vars = []
        for varname in ["CHROOTS_DIR"]:
            if varname in os.environ:
                preserve_vars.append("%s=%s" % (varname, os.environ[varname]))
        command = (["sudo", "env"] + preserve_vars +
                   ["python", sys.argv[0], "--user", "#%i" % os.getuid()]
                   + args)
        os.execvp(command[0], command)
    username = args[1]
    as_superuser = False

    while len(args) > 0 and args[0].startswith("-"):
        option = args.pop(0)
        if option == "--user":
            username = args.pop(0)
        elif option in ("-s", "--su", "--sudo"):
            as_superuser = True
        else:
            raise Error("Unknown option: %s" % option)

    chroot_dir, subdir = get_chroot()
    command = ["chroot", chroot_dir]
    if not as_superuser:
        command.extend(["sudo", "-u", username])
    command.extend(in_dir(subdir, args))
    os.execvp(command[0], command)


def top_main(args):
    try:
        return main(args)
    except Error, exn:
        sys.stderr.write("%s\n" % str(exn))
        return 1


if __name__ == "__main__":
    sys.exit(top_main(sys.argv[1:]))
