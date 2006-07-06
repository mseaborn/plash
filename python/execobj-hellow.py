#!/usr/bin/python

import os
import plash
import plash_marshal
import plash_env

next_inode = [1]

class Exec_obj(plash_marshal.Pyobj_demarshal):
    def __init__(self):
        self.inode = next_inode[0]
        next_inode[0] += 1
    def fsobj_type(self): return m.OBJT_FILE
    def fsobj_stat(self):
        return { 'st_dev': 1, # Could pick a different device number
                 'st_ino': self.inode,
                 'st_mode': stat.S_IFREG | 0777,
                 'st_nlink': 0,
                 'st_uid': 0,
                 'st_gid': 0,
                 'st_rdev': 0,
                 'st_size': 0,
                 'st_blksize': 1024,
                 'st_blocks': 0,
                 'st_atime': 0,
                 'st_mtime': 0,
                 'st_ctime': 0 }
    def eo_is_executable(self): return ()
    def eo_exec(self, args):
        args = dict(args)
        fds = dict(args['Fds.'])
        stdout = fds[1]
        if 'Pgid' in args:
            try:
                pass
                #os.setpgid(0, args[Pgid])
            except:
                import traceback
                traceback.print_exc()
        os.write(stdout.fileno(), "Hello world!\n")
        os.write(stdout.fileno(), "This is an executable object.\n")
        return 0

plash_env.get_return_cont().cap_invoke(('Okay', (Exec_obj(),), ()))
plash.run_server()
print "server exited"
