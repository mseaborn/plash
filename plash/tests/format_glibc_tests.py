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

import re
import sys


known_failures = """
intl/tst-gettext.out: tst-gettext.sh does not invoke executable via wrapper
dirent/tst-fdopendir.out: needs fdopendir (not implemented)
dirent/tst-fdopendir2.out: needs fdopendir (not implemented)
io/tst-openat.out: needs fdopendir (not implemented)
io/tst-unlinkat.out: needs fdopendir (not implemented)
io/tst-fstatat.out: needs fdopendir (not implemented)
io/tst-futimesat.out: needs fdopendir (not implemented)
io/tst-renameat.out: needs fdopendir (not implemented)
io/tst-fchmodat.out: needs fdopendir (not implemented)
io/tst-faccessat.out: needs fdopendir (not implemented)
io/tst-symlinkat.out: needs fdopendir (not implemented)
io/tst-linkat.out: needs fdopendir (not implemented)
io/tst-readlinkat.out: needs fdopendir (not implemented)
io/tst-mkdirat.out: needs fdopendir (not implemented)
io/tst-mknodat.out: needs fdopendir (not implemented)
io/tst-mkfifoat.out: needs fdopendir (not implemented)
"""

known_failures_dict = dict(line.split(": ", 1)
                           for line in known_failures.strip().split("\n"))


# It would be nice if make produced more structured output.
line_regexp = re.compile(r"make\[2\]: \*\*\* \[.*/glibc-build/"
                         "(?P<target>.*?)\]")

def find_failures(stream):
    for line in stream:
        match = line_regexp.match(line)
        if match is not None:
            yield match.group("target")


def main(args):
    if len(args) != 1:
        print "Usage: %s <make-log>" % sys.argv[0]
        return 2
    input_file = args[0]
    failures = list(find_failures(open(input_file, "r")))
    real_failure_list = [target for target in failures
                         if target not in known_failures_dict]
    known_failure_list = [target for target in failures
                          if target in known_failures_dict]
    missing_failure_list = [target for target in known_failures_dict
                            if target not in failures]

    def format_list(name, target_list):
        print "* %i %s" % (len(target_list), name)
        for target in target_list:
            if target in known_failures_dict:
                print "%s: %s" % (target, known_failures_dict[target])
            else:
                print target
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
