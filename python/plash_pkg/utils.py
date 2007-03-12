

def read_file(filename):
    fh = open(filename, "r")
    try:
        return fh.read()
    finally:
        fh.close()

def write_file(filename, data):
    fh = open(filename, "w")
    try:
        fh.write(data)
    finally:
        fh.close()

def join_with_slash(a, b):
    return "%s/%s" % (a.rstrip("/"), b.lstrip("/"))
