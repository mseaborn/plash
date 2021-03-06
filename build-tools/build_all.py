
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

import optparse
import sys

import action_tree
import build_log
import chroot_build
import chroot_build_config


def main(argv):
    parser = optparse.OptionParser()
    parser.add_option("--log-dir", dest="log_dir")
    options, args = parser.parse_args(argv)

    chroots = chroot_build.ChrootSet(chroot_build_config.ChrootConfig())
    if options.log_dir is None:
        log = build_log.DummyLogWriter()
    else:
        logset = build_log.LogSetDir(options.log_dir)
        log = logset.make_logger()
    action_tree.action_main(chroots.incremental_autobuild, args, log=log)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
