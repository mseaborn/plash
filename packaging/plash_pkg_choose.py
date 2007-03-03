#!/usr/bin/python

"""
Usage: plash-pkg-choose <output-dir> <dependency>...
"""

import os
import re
import sys

import plash_pkg


def split_dep_list(text):
    return [x.strip() for x in text.split(",")]

def split_disjunction(text):
    return [x.strip() for x in text.split("|")]

def parse_dep(text):
    m = re.match(r"(\S+)(\s*\([^\)]*\))?$", text)
    if m is None:
        raise Exception("Unrecognised dependency format: %s" % text)
    return {"name": m.group(1)}


class DepChooser(object):

    def __init__(self):
        self._packages_list = []
        self._packages_by_name = {}
        self._chosen_list = []
        self._chosen_set = set()
        self.degree = {}
        self._lacking = []
        self.log = []

    def add_avail_package(self, pkg):
        self._packages_list.append(pkg)
        self._packages_by_name[pkg["package"]] = pkg

    def search_deps(self, name, degree=0):
        """Add a dependency, specified by a single package name."""
        def log(msg):
            self.log.append("%s%s" % ("  " * degree, msg))
        if name not in self._packages_by_name:
            log("%s NOT AVAILABLE" % name)
            self._lacking.append(name)
        else:
            pkg = self._packages_by_name[name]
            if name in self._chosen_set:
                log("(%s %s)" % (name, pkg["version"]))
                self.degree[name] = min(self.degree[name], degree)
            else:
                log("%s %s" % (name, pkg["version"]))
                # Mark the package as visited.
                # It gets added to the list later.
                self._chosen_set.add(name)
                self.degree[name] = degree
                # Process further dependencies
                for field in ("depends", "pre-depends"):
                    if field in pkg:
                        self.process_depends_field(pkg[field],
                                                   degree=degree + 1)
                # Add package to list.
                # We do this last so that the list is topologically sorted,
                # with dependencies first.
                self._chosen_list.append(pkg)

    def process_depends_field(self, dep_line, degree=0):
        for dep in split_dep_list(dep_line):
            disj = split_disjunction(dep)
            # Only look at first dependency in disjunction
            info = parse_dep(disj[0])
            self.search_deps(info["name"], degree)

    # Add packages marked as Essential, since other packages are not
    # required to declare dependencies on them
    def add_essential_packages(self):
        self.log.append("essential packages:")
        for pkg in self._packages_list:
            if ("essential" in pkg and
                pkg["essential"] == "yes"):
                # What about multiple versions of the same package?
                self.search_deps(pkg["package"], degree=1)

    def get_output(self):
        return self._chosen_list

    def warn_missing_dependencies(self):
        if len(self._lacking) > 0:
            print "Missing packages:"
            for name in self._lacking:
                print name


def write_file_seq(filename, seq):
    fh = open(filename, "w")
    try:
        for s in seq:
            fh.write(s)
    finally:
        fh.close()

def write_result_package_list(filename, chooser):
    keys=("package", "version",
          "size", "md5sum", "sha1", "filename", "base-url", "filelist-ref")
    fh = open(filename, "w")
    try:
        for pkg in chooser.get_output():
            plash_pkg.write_block(fh, pkg, keys)
    finally:
        fh.close()


def main(args):
    if len(args) < 1:
        print __doc__
        return
    output_dir = args[0]
    top_deps = args[1:]
    package_list = plash_pkg.get_package_list_combined()
    chooser = DepChooser()

    fh = open(package_list, "r")
    try:
        for block in plash_pkg.file_blocks(fh):
            fields = plash_pkg.block_fields(block)
            chooser.add_avail_package(fields)
    finally:
        fh.close()

    for dep in top_deps:
        chooser.process_depends_field(dep)
    chooser.add_essential_packages()

    if not os.path.exists(output_dir):
        os.mkdir(output_dir)
    chooser.warn_missing_dependencies()
    write_result_package_list(os.path.join(output_dir, "package-list"),
                              chooser)
    write_file_seq(os.path.join(output_dir, "choose-log"),
                   ("%s\n" % line for line in chooser.log))
    # Output dependencies, ordered by degree of separation
    write_file_seq(os.path.join(output_dir, "degrees"),
                   ("%i %s\n" % pair
                    for pair in
                      sorted(((chooser.degree[pkg["package"]], pkg["package"])
                              for pkg in chooser.get_output()),
                             key=lambda (degree, pkg): degree)))


if __name__ == "__main__":
    main(sys.argv[1:])
