
from distutils.core import setup, Extension

setup(name="plash", version="1.0",
      author='Mark Seaborn',
      author_email='mrs@mythic-beasts.com',
      url='http://plash.beasts.org',
      py_modules=['plash_env',
                  'plash_logger',
                  'plash_marshal',
                  'plash_methods', # auto-generated
                  'plash_namespace',
                  'plash_process'],
      ext_modules=[Extension("plash",
                             include_dirs = ["../src", "../gensrc"],
                             library_dirs = ["../obj"],
                             libraries = ["plash", "glib-2.0"],
                             sources = ["py-type-fd.c",
                                        "py-type-obj.c",
                                        "py-functions.c"],
                             )])
