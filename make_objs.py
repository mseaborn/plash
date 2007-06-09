#!/usr/bin/python

# Copyright (C) 2004, 2005, 2006, 2007 Mark Seaborn
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

"""
Usage:  ./make_objs.py [-q] [full_build | warn]

With no arguments, recompiles C files and rebuilds object file archives.

  warn:  Output warnings printed by gcc, most recently built files first
  full_build:  Run ./make.sh if any input files changed
  -q:  quiet

"""

import os
import re
import subprocess
import sys


class CommandFailedError(Exception):

    pass


def run_command(args, **kwargs):
    rc = subprocess.call(args, **kwargs)
    if rc != 0:
        raise CommandFailedError("Return code %i: %s" %
                                 (rc, " ".join(args)))


def get_build_config():
    proc = subprocess.Popen(
        ["sh", "-c", ". src/config.sh && export USE_GTK && export CC && env"],
        stdout=subprocess.PIPE)
    stdout, stderr = proc.communicate()
    assert proc.wait() == 0, proc.wait()
    return dict(line.split("=", 1)
                for line in stdout.split("\n")
                if len(line) > 0)


config = get_build_config()


def get_pkg_config_args():
    proc = subprocess.Popen("pkg-config gtk+-2.0 --cflags", shell=True,
                            stdout=subprocess.PIPE)
    stdout, stderr = proc.communicate()
    assert proc.wait() == 0, proc.wait()
    return stdout.split()


def read_gcc_deps_makefile(filename, dest_file):
    fh = open(filename, "r")
    data = fh.read()
    fh.close()
    data = data.replace("\\\n", "").strip()
    if len(data) > 0:
        m = re.match("(\S+):\d*(.*)$", data)
        assert m is not None
        assert m.group(1) == dest_file
        return m.group(2).split()
    else:
        return []


def needs_rebuilding(dest_file, input_files):
    if not os.path.exists(dest_file):
        return True
    dest_mtime = os.stat(dest_file).st_mtime
    for input_file in input_files:
        if (not os.path.exists(input_file) or
            os.stat(input_file).st_mtime > dest_mtime):
            return True
    return False


class BuildTarget(object):

    def get_descr(self):
        return "build %s" % self.get_dest()

    def get_warnings(self):
        return ""

    def get_dest(self):
        raise NotImplementedError()

    def is_up_to_date(self):
        raise NotImplementedError()

    def build(self):
        raise NotImplementedError()


class GccTarget(BuildTarget):

    def __init__(self, src, dest, options):
        self._src = src
        self._dest = dest
        self._options = options

    def get_dest(self):
        return self._dest

    def get_descr(self):
        return "compile %s -> %s" % (self._src, self.get_dest())

    def _get_deps_file(self):
        return "%s.d" % self._dest

    def is_up_to_date(self):
        if not os.path.exists(self._get_deps_file()):
            return False
        input_files = read_gcc_deps_makefile(self._get_deps_file(), self._dest)
        assert self._src in input_files, self._src
        return not needs_rebuilding(self._dest, input_files)

    def _get_warn_file(self):
        return "%s.err" % self._dest

    def get_warnings(self):
        if os.path.exists(self._get_warn_file()):
            fh = open(self._get_warn_file(), "r")
            try:
                return fh.read()
            finally:
                fh.close()
        else:
            return ""

    def build(self):
        output = open(self._get_warn_file(), "w")
        run_command(config["CC"].split() + self._options +
                    ["-c", self._src,
                     "-o", self._dest,
                     "-MD", "-MF", "%s.d" % self._dest],
                    stdout=output,
                    stderr=output)
        output.close()


class ArArchiveTarget(BuildTarget):

    def __init__(self, dest, obj_files):
        self._dest = dest
        self._obj_files = obj_files

    def get_dest(self):
        return self._dest

    def is_up_to_date(self):
        return not needs_rebuilding(self._dest, self._obj_files)

    def build(self):
        if os.path.exists(self._dest):
            os.unlink(self._dest)
        run_command(["ar", "-cr", self._dest] + self._obj_files)


def get_targets():
    targets = []
    lib_objs_o = []
    lib_objs_os = []

    def gcc(*args):
        target = GccTarget(*args)
        targets.append(target)
        return target

    c_flags = ["-O1", "-Wall", "-Igensrc", "-Isrc"]

    opts_s = c_flags[:]
    if config["USE_GTK"] == "yes":
        opts_s.append("-DPLASH_GLIB")
        opts_s.extend(get_pkg_config_args())

    opts_c = c_flags + ["-D_REENTRANT", "-fPIC"]

    def build_lib(name):
        o_file = gcc("src/%s.c" % name, "obj/%s.o" % name, opts_s)
        os_file = gcc("src/%s.c" % name, "obj/%s.os" % name,
                      opts_s + ["-D_REENTRANT", "-fPIC"])
        lib_objs_o.append(o_file.get_dest())
        lib_objs_os.append(os_file.get_dest())

    build_lib("region")
    build_lib("serialise")
    build_lib("serialise-utils")
    build_lib("comms")
    build_lib("cap-protocol")
    build_lib("cap-call-return")
    build_lib("cap-utils")
    build_lib("cap-utils-libc")
    build_lib("marshal-pack")
    build_lib("marshal-exec")
    build_lib("utils")
    build_lib("parse-filename")
    build_lib("filesysobj")
    build_lib("filesysobj-real")
    build_lib("filesysobj-fab")
    build_lib("filesysobj-readonly")
    build_lib("filesysobj-union")
    build_lib("filesysobj-cow")
    build_lib("filesysslot")
    build_lib("log")
    build_lib("log-proxy")
    build_lib("reconnectable-obj")
    build_lib("build-fs")
    build_lib("build-fs-static")
    build_lib("build-fs-dynamic")
    build_lib("resolve-filename")
    build_lib("fs-operations")
    build_lib("exec")
    build_lib("config-read")
    build_lib("shell-fds")
    build_lib("shell-wait")

    if config["USE_GTK"] == "yes":
        build_lib("powerbox")

    targets.append(ArArchiveTarget("obj/libplash.a", lib_objs_o))
    targets.append(ArArchiveTarget("obj/libplash_pic.a", lib_objs_os))


    # Non-library code

    gcc("src/test-caps.c", "obj/test-caps.o", opts_s)

    gcc("src/shell.c", "obj/shell.o", opts_s)
    gcc("src/shell-parse.c", "obj/shell-parse.o",
        opts_s + ["-Wno-unused"])
    gcc("src/shell-variants.c", "obj/shell-variants.o", opts_s)
    gcc("src/shell-globbing.c", "obj/shell-globbing.o", opts_s)
    gcc("src/shell-options.c", "obj/shell-options.o", opts_s)
    if config["USE_GTK"] == "yes":
        gcc("src/shell-options-gtk.c", "obj/shell-options-gtk.o", opts_s)
    gcc("src/shell-options-cmd.c", "obj/shell-options-cmd.o", opts_s)

    gcc("src/chroot.c", "obj/chroot.o", opts_s)
    gcc("src/exec-object.c", "obj/exec-object.o", opts_s)
    gcc("src/socket-publish.c", "obj/socket-publish.o", opts_s)
    gcc("src/socket-connect.c", "obj/socket-connect.o", opts_s)
    gcc("src/run-emacs.c", "obj/run-emacs.o", opts_s)
    gcc("src/pola-run.c", "obj/pola-run.o", opts_s)
    gcc("src/powerbox-req.c", "obj/powerbox-req.o", opts_s)


    # Build object files to be linked into libc.so and ld.so (ld-linux.so)

    gcc("src/comms.c", "obj/rtld-comms.os", opts_c + ["-DIN_RTLD"])
    # gcc("src/region.c", "obj/region.os", opts_c)
    # gcc("src/serialise.c", "obj/serialise.os", opts_c)
    gcc("src/cap-protocol.c", "obj/libc-cap-protocol.os", opts_c +
        ["-DIN_LIBC"])
    gcc("src/cap-protocol.c", "obj/rtld-cap-protocol.os", opts_c +
        ["-DIN_RTLD"])
    # gcc("src/cap-call-return.c", "obj/cap-call-return.os", opts_c)
    # gcc("src/cap-utils.c", "obj/cap-utils.os", opts_c)
    # gcc("src/marshal-pack.c", "obj/marshal-pack.os", opts_c)
    gcc("src/dont-free.c", "obj/dont-free.os", opts_c)
    # gcc("src/filesysobj.c", "obj/filesysobj.os", opts_c)
    gcc("src/libc-preload-import.c", "obj/libc-preload-import.os", opts_c)
    gcc("src/libc-misc.c", "obj/libc-misc.os", opts_c +
        ["-DIN_LIBC"])
    gcc("src/libc-misc.c", "obj/rtld-libc-misc.os", opts_c +
        ["-DIN_RTLD"])
    gcc("src/libc-stat.c", "obj/libc-stat.os", opts_c + ["-DIN_LIBC"])
    gcc("src/libc-stat.c", "obj/rtld-libc-stat.os", opts_c + ["-DIN_RTLD"])
    gcc("src/libc-comms.c", "obj/libc-comms.os", opts_c + ["-DIN_LIBC"])
    gcc("src/libc-comms.c", "obj/rtld-libc-comms.os", opts_c + ["-DIN_RTLD"])
    gcc("src/libc-fork-exec.c", "obj/libc-fork-exec.os", opts_c)
    gcc("src/libc-connect.c", "obj/libc-connect.os", opts_c)
    gcc("src/libc-getsockopt.c", "obj/libc-getsockopt.os", opts_c)
    gcc("src/libc-getuid.c", "obj/libc-getuid.os", opts_c)
    gcc("src/libc-utime.c", "obj/libc-utime.os", opts_c)
    gcc("src/libc-truncate.c", "obj/libc-truncate.os", opts_c)
    gcc("src/libc-at-calls.c", "obj/libc-at-calls.os", opts_c)
    gcc("src/libc-inotify.c", "obj/libc-inotify.os", opts_c)

    # Build powerbox for Gtk

    if config["USE_GTK"] == "yes":
        opts_gtk_pb = ["-Igensrc", "-Isrc",
                       "-Wall",
                       "-fPIC"] + get_pkg_config_args()
        gcc("src/gtk-powerbox.c", "obj/gtk-powerbox.os", opts_gtk_pb)
        # Not used, but keep building it so that it doesn't bitrot
        gcc("src/gtk-powerbox-noninherit.c", "obj/gtk-powerbox-noninherit.os",
            opts_gtk_pb)

    return targets


def check_no_duplicates(targets):
    target_files = set()
    for target in targets:
        dest = target.get_dest()
        assert dest not in target_files, dest
        target_files.add(dest)


class ProgressLogger(object):

    def visiting_target_pre(self, target, need_rebuild):
        if need_rebuild:
            print target.get_descr()
        else:
            print "up-to-date: %s" % target.get_descr()

    def visiting_target_post(self, target, need_rebuild):
        if need_rebuild:
            sys.stdout.write(target.get_warnings())

    def finish(self):
        pass


class ProgressLoggerQuiet(object):

    def visiting_target_pre(self, target, need_rebuild):
        pass

    def visiting_target_post(self, target, need_rebuild):
        if len(target.get_warnings()) > 0:
            sys.stdout.write("w")
        else:
            sys.stdout.write(".")
        sys.stdout.flush()

    def finish(self):
        sys.stdout.write("\n")


def build(targets, logger):
    did_some_work = False
    for target in targets:
        do_rebuild = not target.is_up_to_date()
        logger.visiting_target_pre(target, do_rebuild)
        if do_rebuild:
            try:
                target.build()
            except:
                sys.stdout.write("\nError building %s\n%s" %
                                 (target.get_dest(), target.get_warnings()))
                raise
            did_some_work = True
        logger.visiting_target_post(target, do_rebuild)
    logger.finish()
    return did_some_work


def show_warnings(targets):
    def get_src_mtime(target):
        return os.stat(target._src).st_mtime

    shown = set()
    for target in reversed(sorted([target for target in targets
                                   if isinstance(target, GccTarget)],
                                  key=get_src_mtime)):
        text = target.get_warnings()
        if text not in shown:
            shown.add(text)
            print text


def main(args):
    targets = get_targets()
    check_no_duplicates(targets)

    logger = ProgressLogger()
    if args[:1] == ["-q"]:
        logger = ProgressLoggerQuiet()
        args.pop(0)

    if args == []:
        build(targets, logger)
    elif args == ["full_build"]:
        if build(targets, logger):
            os.system("./make.sh")
    elif args == ["warn"]:
        show_warnings(targets)
    else:
        print __doc__
        return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
