
# Copyright (C) 2008 Mark Seaborn
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as
# published by the Free Software Foundation; either version 2.1 of the
# License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
# 02110-1301, USA.

import gc
import optparse
import sys

import build_log


def main(argv):
    # Ubuntu Gutsy's version of python-lxml is buggy and sometimes
    # segfaults during GC.  TODO: upgrade and remove this workaround.
    gc.disable()

    parser = optparse.OptionParser()
    options, args = parser.parse_args(argv)
    log_dir, output_file = args
    logset = build_log.LogSetDir(log_dir)
    body = build_log.tag("body")
    for log in logset.get_logs():
        body.append(build_log.format_top_log(log.get_xml(),
                                             build_log.NullPathnameMapper()))
    build_log.write_xml_file(output_file, build_log.wrap_body(body))


if __name__ == "__main__":
    main(sys.argv[1:])
