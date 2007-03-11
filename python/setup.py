
from distutils.core import setup, Extension

setup(name="plash", version="1.0",
      author='Mark Seaborn',
      author_email='mrs@mythic-beasts.com',
      url='http://plash.beasts.org',
      py_modules=['plash.env',
                  'plash.filedesc',
                  'plash.logger',
                  'plash.marshal',
                  'plash.mainloop',
                  'plash.methods', # auto-generated
                  'plash.namespace',
                  'plash.process',
                  'plash.pola_run_args',
                  'plash.powerbox',
                  'plash_pkg',
                  'plash_pkg_choose',
                  ],
      ext_modules=[Extension("plash_core",
                             include_dirs = ["../src", "../gensrc"],
                             library_dirs = ["../obj"],
                             libraries = ["plash_pic", "glib-2.0"],
                             sources = ["py-type-fd.c",
                                        "py-type-obj.c",
                                        "py-functions.c"],
                             )])
