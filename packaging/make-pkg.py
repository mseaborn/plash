
import md5
import os
import string
import tempfile


class InternStream:
    """Calculate a file's hash as we write it, and finally move it
    into place in the intern directory, unless an identical file is
    already there."""

    def __init__(self, hash_dir):
        self._hash_dir = hash_dir
        fd, self._tmp_name = tempfile.mkstemp(suffix="", prefix="tmp",
                                              dir=hash_dir)
        self._fh_out = os.fdopen(fd, "w")
        self._digest = md5.new()
        self._finished = False

    def write(self, data):
        self._fh_out.write(data)
        self._digest.update(data)

    def finish(self):
        """Link the file into place and return its hash."""
        assert not self._finished
        self._finished = True
        file_hash = self._digest.hexdigest()
        dest_path = os.path.join(self._hash_dir, file_hash)
        if not os.path.exists(dest_path):
            os.link(self._tmp_name, dest_path)
        return file_hash

    def close(self):
        """Tidy up: delete temporary file."""
        os.unlink(self._tmp_name)
        self._fh_out.close()

def intern_file(hash_dir, filename):
    """Returns the MD5 hash of the file."""
    fh_in = open(filename, "r")
    out = InternStream(hash_dir)
    try:
        while True:
            data = fh_in.read(4096)
            if len(data) == 0:
                break
            out.write(data)
        return out.finish()
    finally:
        out.close()
        fh_in.close()

def intern_data(hash_dir, data):
    # Writes the file out if the hash is already present -- unnecessary.
    out = InternStream(hash_dir)
    try:
        out.write(data)
        return out.finish()
    finally:
        out.close()

def make_list(hash_dir, files):
    lines = []
    for dest_path, src in files:
        file_hash = intern_file(hash_dir, src)
        lines.append((file_hash, dest_path))
    data = "".join("%s\t%s\n" % pair for pair in lines)
    return intern_data(hash_dir, data)


def make_pb_package():
    ref = make_list("unpack-cache/files",
                    [("/usr/lib/libgtk-powerbox-hook.so",
                      "../shobj/powerbox-for-gtk.so")])
    pkg = {"package": "plash-gtk-hook",
           "version": "0.1-1",
           "filelist-ref": ref}
    print "".join("%s: %s\n" % (string.capitalize(key), val)
                  for key, val in sorted(pkg.iteritems()))


if __name__ == "__main__":
    make_pb_package()
