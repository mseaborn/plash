
from distutils.core import setup, Extension

setup(name="plash", version="1.0",
      ext_modules=[Extension("plash",
                             include_dirs = ["../src", "../gensrc"],
                             library_dirs = ["../obj"],
                             libraries = ["plash", "glib-2.0"],
                             sources = ["py-type-fd.c",
                                        "py-type-obj.c",
                                        "py-functions.c"],
                             )])
