
import re
import string


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
    d = {}
    for line in lines:
        m = re.compile(r"^(\S+?): ?(.*)$").match(line)
        key, val = m.group(1, 2)
        d[string.lower(key)] = val
    return d
