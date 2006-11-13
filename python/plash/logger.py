
import plash.marshal
import traceback

indent = ['>']

# Logging proxy object
class logger(plash.marshal.Pyobj_marshal):
    def __init__(self, x1): self.x = x1
    def cap_call(self, args):
        print indent[0], "call",
        try:
            print plash.marshal.unpack(args)
        except:
            print args, "exception:",
            traceback.print_exc()
        try:
            i = indent[0]
            indent[0] = i + '  '
            try:
                r = self.x.cap_call(args)
            finally:
                indent[0] = i
        except:
            print indent[0], "Exception in cap_call:",
            traceback.print_exc()
            return
        print indent[0], "->",
        try:
            print plash.marshal.unpack(r)
        except:
            print r
        return r
