
import re


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
