# Copyright (C) 2006, 2007 Mark Seaborn
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
import string


class BadControlLineError(Exception):
    pass


def get_block(file):
    lines = []
    while True:
        line = file.readline()
        if line == "" and len(lines) == 0:
            raise EOFError()
        if line == "" or line == "\n":
            return lines
        lines.append(line)

def file_blocks(file):
    """Returns an iterable of lists of lines."""
    try:
        while True:
            yield get_block(file)
    except EOFError:
        pass

def block_fields(lines):
    """
    Convert a Debian control block, represented as a list of lines,
    into a dictionary.
    """
    regexp_line0 = re.compile(r"^(\S+?): ?(.*)$")
    fields = {}
    current_key = None
    for line in lines:
        match = regexp_line0.match(line)
        if match:
            key, val = match.group(1, 2)
            current_key = string.lower(key)
            assert current_key not in fields
            fields[current_key] = val
        elif line.startswith(" ") and current_key is not None:
            fields[current_key] = fields[current_key] + "\n" + line[1:]
        else:
            raise BadControlLineError(line)
    return fields

def write_block(fh, fields, keys=None):
    if keys is None:
        for key, value in sorted(fields.iteritems()):
            fh.write("%s: %s\n" % (key.capitalize(), value))
    else:
        for key in keys:
            if key in fields:
                fh.write("%s: %s\n" % (key.capitalize(), fields[key]))
    fh.write("\n")
