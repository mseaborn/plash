
import os
import re
import string


class BadControlLineError(Exception):
    pass


def join_with_slash(a, b):
    return "%s/%s" % (a.rstrip("/"), b.lstrip("/"))


def ensure_dir_exists(dir_path):
    if not os.path.exists(dir_path):
        os.mkdir(dir_path)
    return dir_path

def get_arch():
    return "i386"

def get_package_list_cache_dir():
    return ensure_dir_exists("package-lists")

def get_package_list_combined():
    return os.path.join(get_package_list_cache_dir(),
                        "Packages.combined")

def get_deb_cache_dir():
    """Directory where downloaded .deb files are stored."""
    return ensure_dir_exists("cache")

def get_unpack_cache_dir():
    """Directory where unpacked copies of packages are stored."""
    return ensure_dir_exists("unpack-cache")


def read_sources_list_line(line):
    """
    Parse a line from an APT sources.list file.
    """
    # Ignore comments and empty lines
    if (re.compile("\s*$").match(line) or
        re.compile("\s*#").match(line)):
        pass
    else:
        # Handle a "deb" line
        match = re.compile("\s* deb \s+ (.+)$", re.X).match(line)
        if match:
            args = re.split("\s+", match.group(1))
            if len(args) == 0:
                raise Exception("Malformed deb line: "
                                "URL missing: %s" % line)
            if len(args) == 1:
                raise Exception("Malformed deb line: "
                                "distribution name missing: %s" % line)
            url = args.pop(0)
            dist = args.pop(0)
            if "/" in dist and len(args) == 0:
                yield {"S_base": url, "S_component": dist}
            else:
                if len(args) == 0:
                    raise Exception("Malformed deb line: "
                                    "no component names: %s" % line)
                for component in args:
                    yield {"S_base": url,
                           "S_component": "dists/%s/%s" % (dist, component)}
        else:
            # Ignore "deb-src" lines
            if re.compile("\s* deb-src \s+", re.X).match(line):
                pass
            else:
                raise Exception("Unrecognised sources.list line: %s" % line)

def read_sources_list(filename):
    fh = open(filename, "r")
    try:
        return [item
                for line in fh
                for item in read_sources_list_line(line)]
    finally:
        fh.close()


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
