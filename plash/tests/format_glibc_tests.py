#!/usr/bin/env python

# Copyright (C) 2008 Mark Seaborn
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

import optparse
import os
import re
import sys


def parse_known_failures(text):
    return dict(line.split(": ", 1)
                for line in text.strip().split("\n"))


known_failures_dict = parse_known_failures(
# Cause known.  Fixable.
"""\
dirent/bug-readdir1.out: possibly close(-1) not working as expected
dirent/tst-fdopendir.out: needs fdopendir (not implemented)
dirent/tst-fdopendir2.out: needs fdopendir (not implemented)
intl/tst-gettext.out: tst-gettext.sh does not invoke executable via wrapper
io/test-lfs.out: stat64() needs fixing
io/tst-faccessat.out: needs fdopendir (not implemented)
io/tst-fchmodat.out: needs fdopendir (not implemented)
io/tst-fstatat.out: needs fdopendir (not implemented)
io/tst-futimesat.out: needs fdopendir (not implemented)
io/tst-linkat.out: needs fdopendir (not implemented)
io/tst-mkdirat.out: needs fdopendir (not implemented)
io/tst-mkfifoat.out: needs fdopendir (not implemented)
io/tst-mknodat.out: needs fdopendir (not implemented)
io/tst-openat.out: needs fdopendir (not implemented)
io/tst-readlinkat.out: needs fdopendir (not implemented)
io/tst-renameat.out: needs fdopendir (not implemented)
io/tst-symlinkat.out: needs fdopendir (not implemented)
io/tst-unlinkat.out: needs fdopendir (not implemented)
nptl/tst-cancel4.out: probably a problem cancelling close()
nptl/tst-cancel5.out: probably a problem cancelling close()
nptl/tst-cancelx4.out: probably a problem cancelling close()
nptl/tst-cancelx5.out: probably a problem cancelling close()
"""

# Cause known.  Harder to fix.
"""\
nptl/tst-umask1.out: umask() and mkfifo() are not supported
posix/tst-chmod.out: errno from creat(); use of dirfd()
posix/tst-dir.out: use of dirfd()
"""

# Probably problems caused by exec'ing subprocesses or using static
# linking.
"""\
dlfcn/tststatic.out: ??
dlfcn/tststatic2.out: ??
elf/order.out: ??
elf/order2.out: ??
elf/tst-pathopt.out: ??
elf/tst-stackguard1-static.out: ??
elf/tst-tls1-static.out: ??
elf/tst-tls2-static.out: ??
elf/tst-tls9-static.out: ??
io/bug-ftw2.out: see io/ftwtest-sh
io/bug-ftw4.out: see io/ftwtest-sh
io/ftwtest.out: see io/ftwtest-sh
malloc/tst-mtrace.out: see malloc/tst-mtrace.sh
nptl/tst-cancel7.out: nested wrapper invocation
nptl/tst-cancelx7.out: nested wrapper invocation
nptl/tst-exec4.out: nested wrapper invocation
nptl/tst-locale1.out: Cannot fork test program: Function not implemented
nptl/tst-locale2.out: Cannot fork test program: Function not implemented
nptl/tst-stackguard1-static: duplicate symbol, can't statically link
posix/globtest.out: see globtest.sh
posix/tst-exec.out: ??
posix/tst-execl2.out: ??
posix/tst-execle2.out: ??
posix/tst-execlp2.out: ??
posix/tst-execv2.out: ??
posix/tst-execve2.out: ??
posix/tst-execvp2.out: ??
posix/tst-spawn.out: ??
posix/tst-vfork3-mem: ??
posix/wordexp-tst.out: see posix/wordexp-tst.sh
rt/tst-mqueue7.out: nested wrapper invocation
"""

# Cause unknown.
"""\
elf/tst-audit1.out: segfault
elf/tst-audit2.out: segfault
elf/tst-stackguard1.out: "stack guard canaries are not randomized enough"
grp/testgrp.out: ??
grp/tst_fgetgrent.out: ??
libio/tst-fopenloc.check: ??
localedata/mtrace-tst-leaks: ??
login/tst-utmp.out: ??
login/tst-utmpx.out: ??
nptl/tst-attr3.out: pthread_getattr_np fails (does it need /proc?)
nptl/tst-stackguard1.out: "stack guard canaries are not randomized enough"
""")


# Keep the reasons for past failures around to aid debugging in case
# the failures re-occur.
past_failures_dict = parse_known_failures("""\
debug/tst-chk4: /usr/bin/ld: cannot find -lstdc++
debug/tst-chk5: /usr/bin/ld: cannot find -lstdc++
debug/tst-chk6: /usr/bin/ld: cannot find -lstdc++
debug/tst-lfschk4: /usr/bin/ld: cannot find -lstdc++
debug/tst-lfschk5: /usr/bin/ld: cannot find -lstdc++
debug/tst-lfschk6: /usr/bin/ld: cannot find -lstdc++
dlfcn/bug-atexit3-lib.so: /usr/bin/ld: cannot find -lstdc++
nptl/tst-cancel24: /usr/bin/ld: cannot find -lstdc++
rt/tst-cpuclock2.out: ??
""")


failure_reasons = past_failures_dict.copy()
failure_reasons.update(known_failures_dict)


# It would be nice if make produced more structured output.
line_regexp = re.compile(r"make\[2\]: \*\*\* \[.*/glibc-build/"
                         "(?P<target>.*?)\]")

def find_failures(stream):
    for line in stream:
        match = line_regexp.match(line)
        if match is not None:
            yield match.group("target")


def print_test_output(filename):
    fh = open(filename, "r")
    for line_number, line in enumerate(fh):
        if line_number > 100:
            print "(truncated)"
            break
        else:
            print ">" + line.rstrip("\n")
    fh.close()


def main(argv):
    parser = optparse.OptionParser("Usage: %prog <make-log>")
    parser.add_option("-v", "--verbose", dest="verbose", default=False,
                      action="store_true",
                      help="Show more information about failures")
    options, args = parser.parse_args(argv)
    if len(args) != 1:
        parser.error("Expected log argument")
    input_file = args[0]
    build_dir = "glibc-build"
    failures = list(find_failures(open(input_file, "r")))
    real_failure_list = [target for target in failures
                         if target not in known_failures_dict]
    known_failure_list = [target for target in failures
                          if target in known_failures_dict]
    missing_failure_list = [target for target in known_failures_dict
                            if target not in failures]

    def format_list(name, target_list):
        print "* %i %s" % (len(target_list), name)
        for target in sorted(target_list):
            if target in failure_reasons:
                print "%s: %s" % (target, failure_reasons[target])
            else:
                print target
            if options.verbose:
                output_file = os.path.join(build_dir, "%s.temp" % target)
                if os.path.exists(output_file):
                    print_test_output(output_file)
                print
        print

    format_list("unexpected failures", real_failure_list)
    format_list("known failures that did not occur", missing_failure_list)
    format_list("known failures", known_failure_list)

    if len(real_failure_list) != 0 or len(missing_failure_list) != 0:
        return 1
    else:
        return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
