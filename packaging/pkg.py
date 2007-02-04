
import re
import string


def get_block(file):
    lines = []
    while True:
        line = file.readline()
        if line == "" or line == "\n":
            return lines
        lines.append(line)

def block_fields(lines):
    d = {}
    for line in lines:
        m = re.compile(r"^(\S+?): ?(.*)$").match(line)
        key, val = m.group(1, 2)
        d[string.lower(key)] = val
    return d
