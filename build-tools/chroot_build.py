
# Copyright (C) 2007 Mark Seaborn
#
# chroot_build is free software; you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as
# published by the Free Software Foundation; either version 2.1 of the
# License, or (at your option) any later version.
#
# chroot_build is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with chroot_build; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
# 02110-1301, USA.

import glob
import itertools
import md5
import os
import pwd
import shutil
import subprocess
import sys
import time
import traceback

from buildutils import remove_prefix
import action_tree
import build_log
import chroot_build_config

# Package dependencies: debootstrap, python-lxml
# Need the version of debootstrap from hardy in order to debootstrap hardy.


# To stop Perl (running inside chroot) warning endlessly about locales.
os.environ["LANG"] = "C"


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


def get_one(items):
    assert len(items) == 1, items
    return items[0]


def make_pipe():
    pipe_read, pipe_write = os.pipe()
    return (os.fdopen(pipe_read), os.fdopen(pipe_write, "w"))


class CommandFailedError(Exception):

    pass


def run_cmd_logged(log, args, **kwargs):
    t0 = time.time()
    print " ".join(args), kwargs
    log_file = log.make_file()
    log_file.write("%r\n" % args)
    pipe_read, pipe_write = make_pipe()
    stdout = kwargs.pop("stdout", pipe_write)
    stderr = kwargs.pop("stderr", pipe_write)
    proc = subprocess.Popen(args, stdin=open("/dev/null", "r"),
                            stdout=stdout, stderr=stderr, **kwargs)
    pipe_write.close()
    while True:
        # Use os.read() instead of proc.stdout.read() to avoid buffering.
        data = os.read(pipe_read.fileno(), 1024)
        if len(data) == 0:
            break
        log_file.write(data)
        sys.stdout.write(data)
    log_file.close()
    rc = proc.wait()
    t1 = time.time()
    took = t1 - t0
    if took > 3:
        print "(took %im%.1fs)" % (took / 60, took % 60)
    if rc != 0:
        raise CommandFailedError("Command exited with status %i: %s" %
                                 (rc, args))


def run_cmd_unlogged(args, **kwargs):
    print " ".join(args), kwargs
    rc = subprocess.call(args, **kwargs)
    if rc != 0:
        raise CommandFailedError("Command exited with status %i: %s" %
                                 (rc, args))


def run_cmd(log, args, **kwargs):
    if isinstance(log, build_log.DummyLogWriter):
        run_cmd_unlogged(args, **kwargs)
    else:
        run_cmd_logged(log, args, **kwargs)


def in_dir(dir_path, args):
    return ["sh", "-c", 'cd "$1" && shift && exec "$@"',
            "x", dir_path] + args


def write_file_cmd(filename, data):
    return ["sh", "-c", 'echo "$1" >"$2"', "write_file_script",
            data, filename]


def get_all_mounts():
    fh = open("/proc/mounts", "r")
    for line in fh:
        parts = line.split()
        yield parts[1]
    fh.close()

def is_mounted(path):
    return os.path.realpath(path) in list(get_all_mounts())


def svn_update(log, run_command, dir_path):
    # svn often fails so we need to retry
    for i in range(5):
        try:
            run_command(log, ["svn", "update"], cwd=dir_path)
        except CommandFailedError:
            time.sleep(5)
        else:
            break


def log_last_exception(log):
    log_file = log.make_file()
    try:
        exc_type, value, trace = sys.exc_info()
        message = "".join(traceback.format_exception(exc_type, value, trace))
        log_file.write(message)
        sys.stdout.write(message)
    except:
        log_file.close()


class Action(object):

    def __init__(self, name, func):
        self._func = func
        self.__name__ = name

    def __call__(self, log):
        self._func(log)


def mkdir_p(dir_path):
    if not os.path.exists(dir_path):
        os.mkdir(dir_path)

def symlink_p(dest_path, symlink_path):
    if not os.path.lexists(symlink_path):
        os.symlink(dest_path, symlink_path)


# TODO: remove this mixin and use LogSetDir directly.
class LogDirMixin(object):

    def get_log_set(self):
        raise NotImplementedError()

    def get_logs(self):
        logs = self.get_log_set().get_logs()
        return list(itertools.islice(logs, 20))


def chroot_path(chroot_dir, path):
    return "/" + remove_prefix(chroot_dir.rstrip("/") + "/", path)


def relative_path(parent_dir, dest_path):
    # TODO: support adding ".."
    return remove_prefix(parent_dir.rstrip("/") + "/", dest_path)


class Debootstrapper(object):

    def __init__(self, debian_repo_url, dist, arch, subdists, script):
        self._debian_repo_url = debian_repo_url
        self._dist = dist
        self._arch = arch
        self._subdists = subdists
        self._script = script

    def make_chroot(self, log, dest_dir):
        args = ["sudo", "debootstrap"]
        if self._arch is not None:
            args.extend(["--arch", self._arch])
        args.extend([self._dist, dest_dir, self._debian_repo_url])
        if self._script is not None:
            args.append(self._script)
        run_cmd(log, args)

    def get_dist(self):
        return self._dist

    def get_deb_line(self):
        return "deb %s %s %s\n" % (self._debian_repo_url, self._dist,
                                   " ".join(self._subdists))


class StampDir(object):

    def __init__(self, stamp_dir):
        self._stamp_dir = stamp_dir

    def init(self, log):
        mkdir_p(self._stamp_dir)

    def wrap_action(self, name, func):
        stamp_file = os.path.join(self._stamp_dir, name)

        def wrapper(log):
            if os.path.exists(stamp_file):
                log.message("skipped: stamp file exists")
            else:
                func(log)
                fh = open(stamp_file, "w")
                fh.close()

        return Action(name, wrapper)


class ChrootEnv(object):

    def __init__(self, chroot_dir, stamp_dir, username, debootstrapper):
        self._chroot_dir = chroot_dir
        self._stamp_dir = stamp_dir
        self._username = username
        self._debootstrapper = debootstrapper
        self._workdir = "work"

    def get_dest(self):
        return self._chroot_dir

    def get_workdir(self):
        return self._workdir

    def get_debootstrapper(self):
        return self._debootstrapper

    def make_chroot(self, log):
        self._debootstrapper.make_chroot(log, self.get_dest())

    def chroot_path(self, pathname):
        return chroot_path(self.get_dest(), pathname)

    def in_chroot_root(self, log, cmd_args, cwd=None):
        if cwd is not None:
            cmd_args = in_dir(self.chroot_path(cwd), cmd_args)
        run_cmd(log, ["sudo", "chroot", self.get_dest()] + cmd_args)

    def in_chroot(self, log, cmd_args, cwd=None):
        self.in_chroot_root(log, ["sudo", "-u", self._username] + cmd_args,
                            cwd=cwd)

    def run_cmd_as_normal_user(self, log, args, **kwargs):
        run_cmd(log, ["sudo", "-u", self._username] + args, **kwargs)

    def install_base(self, log):
        # We need sudo, which is in base on Ubuntu but not on Debian
        self.in_chroot_root(log, ["env", "DEBIAN_FRONTEND=noninteractive",
                                  "apt-get", "-y", "--force-yes", "install",
                                  "build-essential", "pbuilder", "fakeroot",
                                  "sudo"])

    def set_up_etc_hosts(self, log):
        self.in_chroot_root(log, write_file_cmd("/etc/hosts",
                                                "127.0.0.1 localhost\n"))

    def create_user(self, log):
        uid = pwd.getpwnam(self._username).pw_uid
        self.in_chroot_root(
            log, ["adduser", "--uid", str(uid),
                  "--home", os.path.join("/", self._workdir),
                  "--disabled-password", "--gecos", "",
                  self._username])

    def mount_proc_fs(self, log):
        dest_path = os.path.join(self.get_dest(), "proc")
        if not is_mounted(dest_path):
            run_cmd(log, ["sudo", "mount", "-t", "proc",
                          "proc", dest_path])

    def bind_mount_dev(self, log):
        # /dev/ptmx and /dev/pts are needed for openpty() to work.
        dest_path = os.path.join(self.get_dest(), "dev")
        if not is_mounted(dest_path):
            run_cmd(log, ["sudo", "mount", "--rbind", "/dev", dest_path])

    def bind_x11(self, log):
        dest_path = os.path.join(self.get_dest(), "tmp/.X11-unix")
        if not is_mounted(dest_path):
            run_cmd(log, ["sudo", "mount", "--bind", "/tmp/.X11-unix",
                          dest_path])

    def remove_mounts(self, log):
        dir_path = os.path.realpath(self.get_dest())
        mounts = [pathname
                  for pathname in get_all_mounts()
                  if pathname.startswith(dir_path)]
        # Need to reverse sort to ensure that, for example, /dev/pts
        # is unmounted before /dev.
        for pathname in sorted(mounts, reverse=True):
            run_cmd(log, ["sudo", "umount", pathname])

    @action_tree.action_node
    def set_up(self):
        return [self._stamp_dir.init,
                self._stamp_dir.wrap_action("make_chroot", self.make_chroot),
                self.mount_proc_fs,
                self.bind_mount_dev,
                self._stamp_dir.wrap_action("install_base", self.install_base),
                self.set_up_etc_hosts,
                self._stamp_dir.wrap_action("create_user", self.create_user)]

    def shell(self, log):
        prompt = r"%s:\w\$ " % self._name
        self.in_chroot(log, ["env", "PS1=%s" % prompt,
                             "HOME=%s" % os.path.join("/", self._workdir),
                             "bash", "--noprofile", "--norc"])

    def root_shell(self, log):
        prompt = r"%s:\w\$ " % self._name
        self.in_chroot_root(log, ["env", "PS1=%s" % prompt,
                                  "bash", "--noprofile", "--norc"])

    def tear_down(self, log):
        self.remove_mounts(log)
        run_cmd(log, ["sudo", "rm", "-rf", self.get_dest()])


class Builder(LogDirMixin):

    def __init__(self, chroot, base_dir, stamp_dir, name, config, deb_versions):
        self.chroot = chroot
        self._base_dir = base_dir
        self._stamp_dir = stamp_dir
        self.dist = chroot.get_debootstrapper().get_dist()
        self._name = name
        self._template = config.plash_svn_template
        self._config = config
        top_dir = os.path.join(self.chroot.get_workdir(), "plash")
        self._top_abs_dir = os.path.join(self.get_dest(), top_dir)
        self._jail_module = DebianModule(
            self.chroot, deb_versions, "plash-chroot-jail",
            os.path.join(top_dir, "chroot-jail"))
        self._plash_module = PlashDebianModule(
            self.chroot, deb_versions, "plash",
            os.path.join(top_dir, "plash"))
        self._gtkhook_module = DebianModule(
            self.chroot, deb_versions, "gtk-powerbox-hook",
            os.path.join(top_dir, "gtk-powerbox-hook"))
        self._modules = [self._jail_module,
                         self._plash_module,
                         self._gtkhook_module]

    def make_dirs(self):
        mkdir_p(self._base_dir)
        mkdir_p(self.get_dest())
        symlink_p(relative_path(self._base_dir, self.get_working_dir()),
                  os.path.join(self._base_dir, "plash"))

    @action_tree.action_node
    def all_steps(self):
        return [
            self.chroot.set_up,
            self._stamp_dir.wrap_action("copy_source", self.copy_source),
            self.svn_update,
            self.add_local_debian_repo,
            ("chroot-jail", self._jail_module.build_generic_deb),
            self.clone_glibc_source,
            self._plash_module.build_plash,
            ("gtk-powerbox-hook", self._gtkhook_module.build_generic_deb)]

    def get_dest(self):
        return os.path.join(self._base_dir, "chroot")

    def get_log_set(self):
        return build_log.LogSetDir(os.path.join(self._base_dir, "logs"))

    def get_working_dir(self):
        return os.path.join(self.get_dest(), self.chroot.get_workdir(), "plash")

    def get_name(self):
        return self._name

    def clone_glibc_source(self, log):
        glibc_source_dir = os.path.join(self._top_abs_dir, "plash",
                                        "glibc-source")
        if not os.path.exists(glibc_source_dir):
            # Might consider using --local, which hard links Git
            # object data, instead of --shared.
            self.chroot.run_cmd_as_normal_user(
                log, ["git-clone", "--shared", self._config.glibc_git_repo,
                      glibc_source_dir])
        else:
            self.chroot.run_cmd_as_normal_user(log, ["git-pull"],
                                               cwd=glibc_source_dir)

    def copy_source(self, log):
        if self._config.enable_svn:
            svn_update(log, self.chroot.run_cmd_as_normal_user, self._template)
        dest = self._top_abs_dir
        assert not os.path.exists(dest)
        run_cmd(log, ["cp", "-a", os.path.realpath(self._template), dest])

    def svn_update(self, log):
        if self._config.enable_svn:
            svn_update(log, self.chroot.run_cmd_as_normal_user,
                       self._top_abs_dir)

    def add_local_debian_repo(self, log):
        self.chroot.in_chroot_root(
            log, write_file_cmd(
                "/etc/apt/sources.list.d/plash.list",
                self.chroot.get_debootstrapper().get_deb_line() +
                self._config.plash_debian_repo))
        self.chroot.in_chroot_root(log, ["apt-get", "update"])

    def get_built_debs(self):
        return [deb_file
                for module in self._modules
                for deb_file in module.get_built_debs()]

    def tests_passed(self):
        return self._plash_module.tests_passed()


class DebianModule(object):

    def __init__(self, chroot, deb_versions, package_name, subdir):
        self.chroot = chroot
        self._deb_versions = deb_versions
        self._package_name = package_name
        self._module_dir = subdir
        self._module_abs_dir = os.path.join(chroot.get_dest(), subdir)

    @action_tree.action_node
    def build_generic_deb(self):
        return [
            self.update_changelog,
            self.install_deps,
            self.build,
            self.run_lintian,
            self.install_debs]

    def install_deps(self, log):
        # strace is needed for running tests
        self.chroot.in_chroot_root(
            log, ["apt-get", "-y", "--force-yes", "install", "strace"])
        self.chroot.in_chroot_root(log,
            in_dir(self._module_dir,
                   ["/usr/lib/pbuilder/pbuilder-satisfydepends"]))

    def build(self, log):
        self.clean_built_debs(log)
        self.chroot.in_chroot(
            log, cwd=self._module_abs_dir,
            cmd_args=["dpkg-buildpackage", "-rfakeroot", "-b", "-D"])

    def build_from_debian_source(self, log):
        self.chroot.in_chroot(
            log, cwd=self._module_abs_dir,
            cmd_args=["dpkg-buildpackage", "-rfakeroot", "-b"])

    def clean_built_debs(self, log):
        for deb_file in self.get_built_debs():
            os.unlink(deb_file)

    def get_built_debs(self):
        parent_dir = os.path.dirname(self._module_abs_dir)
        debian_list_file = os.path.join(self._module_abs_dir, "debian", "files")
        if os.path.exists(debian_list_file):
            deb_files = []
            fh = open(debian_list_file)
            for line in fh:
                leafname = line.split(" ")[0]
                pathname = os.path.join(parent_dir, leafname)
                assert os.path.exists(pathname), (pathname, line)
                deb_files.append(pathname)
            return deb_files
        else:
            return []

    def install_debs(self, log):
        deb_files = [self.chroot.chroot_path(deb_file)
                     for deb_file in self.get_built_debs()]
        self.chroot.in_chroot_root(log, ["dpkg", "-i"] + deb_files)

    def run_lintian(self, log):
        self.chroot.in_chroot_root(log, ["apt-get", "-y", "--force-yes",
                                         "install", "lintian"])
        deb_files = [self.chroot.chroot_path(deb_file)
                     for deb_file in self.get_built_debs()]
        try:
            self.chroot.in_chroot(
                log, in_dir(self._module_dir, ["lintian"] + deb_files))
        except CommandFailedError:
            # On edgy, lintian returns an error code for warnings.
            # TODO: fix all lintian warnings.
            if self.chroot.get_debootstrapper().get_dist() != "edgy":
                raise

    def update_changelog(self, log):
        dist = self.chroot.get_debootstrapper().get_dist()
        write_file(os.path.join(self._module_abs_dir, "debian", "changelog"),
                   self._deb_versions.make_changelog(self._package_name, dist))


class PlashDebianModule(DebianModule):

    @action_tree.action_node
    def build_plash(self):
        yield self.clean
        yield self.setup_debian_symlink
        yield self.update_changelog
        yield self.install_deps
        yield self.build
        yield self.test
        yield self.run_lintian
        yield self.install_debs
        yield self.test_mostly_installed
        dist = self.chroot.get_debootstrapper().get_dist()
        if dist == "lenny":
            yield self.glibc_tests
            yield self.glibc_tests_summary

    def clean(self, log):
        # TODO: once build dirs can be separate from source dir, we
        # won't need to do this
        dest = self._module_abs_dir
        self.chroot.run_cmd_as_normal_user(
            log, ["rm", "-rf",
                  os.path.join(dest, "debian/files"),
                  os.path.join(dest, "obj"),
                  os.path.join(dest, "shobj"),
                  os.path.join(dest, "gensrc"),
                  os.path.join(dest, "configure"),
                  # os.path.join(dest, "stamp-configure-glibc"),
                  os.path.join(dest, "stamp-build-glibc"),
                  os.path.join(dest, "stamp-build-plash"),
                  os.path.join(dest, "stamp-tests-passed"),
                  ])

    def setup_debian_symlink(self, log):
        # TODO: could rename "debian-template" to "debian" now that we
        # only have one debian dir
        self.chroot.in_chroot(
            log,
            in_dir(self._module_dir,
                   ["ln", "-sfvn", "debian-template", "debian"]))

    def test(self, log):
        self.chroot.in_chroot(log,
            in_dir(self._module_dir,
                   ["./run-uninstalled.sh", "python",
                    "tests/all_tests.py", "-v"]))
        write_file(os.path.join(self._module_abs_dir, "stamp-tests-passed"),
                   "")

    def tests_passed(self):
        return os.path.exists(os.path.join(self._module_abs_dir,
                                           "stamp-tests-passed"))

    def test_mostly_installed(self, log):
        self.chroot.in_chroot(log,
            in_dir(self._module_dir,
                   ["./run-mostly-installed.sh", "python",
                    "tests/all_tests.py", "-v"]))

    def glibc_tests(self, log):
        self.chroot.in_chroot(
            log, in_dir(self._module_dir,
                        ["bash", "-c",
                         "./build-glibc-integrated.sh clean_tests test 2>&1 "
                         "| tee test-log"]))

    def glibc_tests_summary(self, log):
        self.chroot.in_chroot(
            log, in_dir(self._module_dir,
                        ["sh", "-c",
                         "./tests/format_glibc_tests.py test-log"]))


def index_repo(log, repo_dir, variant_name):
    index_dir = os.path.join(repo_dir, variant_name)
    os.mkdir(index_dir)
    fh = open(os.path.join(index_dir, "Packages"), "w")
    run_cmd(log, ["dpkg-scanpackages", "-m", ".", "/dev/null"],
            cwd=repo_dir, stdout=fh)
    fh.close()
    fh = open(os.path.join(index_dir, "Sources"), "w")
    run_cmd(log, ["dpkg-scansources", "."],
            cwd=repo_dir, stdout=fh)
    fh.close()
    run_cmd(log, ["gzip", os.path.join(index_dir, "Packages")])
    run_cmd(log, ["gzip", os.path.join(index_dir, "Sources")])


def file_list_rec(top_dir_path):
    got = []
    
    def traverse(full_path, rel_path):
        if os.path.islink(full_path):
            pass
        elif os.path.isfile(full_path):
            got.append(rel_path)
        elif os.path.isdir(full_path):
            for leafname in os.listdir(full_path):
                traverse(os.path.join(full_path, leafname),
                         os.path.join(rel_path, leafname))

    for leafname in os.listdir(top_dir_path):
        traverse(os.path.join(top_dir_path, leafname),
                 leafname)
    return got

def get_file_hash(filename):
    digest = md5.new()
    fh = open(filename, "r")
    try:
        digest.update(fh.read(4096))
        return digest.hexdigest()
    finally:
        fh.close()

def reset_dir(dir_path):
    if os.path.exists(dir_path):
        shutil.rmtree(dir_path)
    os.mkdir(dir_path)

def move_dir_onto(source_dir, dest_dir):
    assert os.path.exists(source_dir)
    if os.path.exists(dest_dir):
        temp_dir = "%s.to-be-deleted" % dest_dir
        os.rename(dest_dir, temp_dir)
        os.rename(source_dir, dest_dir)
        shutil.rmtree(temp_dir)
    else:
        os.rename(source_dir, dest_dir)

def copy_dir(log, src, dest):
    for leaf in os.listdir(src):
        run_cmd(log, ["cp", "-a", os.path.join(src, leaf), dest])


# We go to some trouble to re-use the same tar file for the variants
# of a source package.  Unfortunately tar is not deterministic.
# Timestamps and file ordering can change.
def make_source_packages(config, subdir_name, package_name,
                         version, variants, out_dir):
    # Make .orig dir
    orig_leafname = "%s-%s.orig" % (package_name, version)
    run_cmd_unlogged(["svn", "export",
                      os.path.join(config.plash_svn_template, subdir_name),
                      os.path.join(out_dir, orig_leafname)])
    # Get glibc patch
    # Put this in .orig because it is not part of the Debian packaging
    # and is "upstream" (though that is meaningless because there is no
    # upstream/deb distinction here), although it is not part of the Plash
    # module.
    if package_name == "plash":
        run_cmd_unlogged(
            ["git-diff", "glibc-2.7", "HEAD"],
            cwd=config.glibc_git_repo,
            stdout=open(os.path.join(out_dir, orig_leafname,
                                     "glibc-2.7-plash.patch"), "w"))
    # Make .orig tarball
    run_cmd_unlogged(
        ["tar", "-C", out_dir, "-czf",
         os.path.join(out_dir, "%s_%s.orig.tar.gz" % (package_name, version)),
         orig_leafname])

    for subdir, changelog_text in variants:
        mkdir_p(subdir)
        run_cmd_unlogged(
            ["cp", "-lr",
             os.path.join(out_dir, "%s-%s.orig" % (package_name, version)),
             os.path.join(out_dir, "%s_%s.orig.tar.gz" %
                          (package_name, version)),
             subdir])

        run_cmd_unlogged(
            ["cp", "-ar",
             os.path.join(subdir, "%s-%s.orig" % (package_name, version)),
             os.path.join(subdir, "%s-%s" % (package_name, version))])
        # Set up debian dir
        if package_name == "plash":
            run_cmd_unlogged(
                ["cp", "-ar",
                 os.path.join(subdir, "plash-%s" % version, "debian-template"),
                 os.path.join(subdir, "plash-%s" % version, "debian")])
        write_file(os.path.join(subdir, "%s-%s" % (package_name, version),
                                "debian/changelog"),
                   changelog_text)
        # The -ss option tells dpkg-source that both the directory and
        # the tarfile are available.  It uses the directory for the
        # diff and the tar for the hash.
        run_cmd_unlogged(["dpkg-source", "-b", "-ss",
                          "%s-%s" % (package_name, version)],
                         cwd=subdir)
        # Tidy up: unpacked trees not needed
        shutil.rmtree(os.path.join(subdir, "%s-%s.orig" % (package_name,
                                                           version)))
        shutil.rmtree(os.path.join(subdir, "%s-%s" % (package_name, version)))
    # Tidy up
    shutil.rmtree(os.path.join(out_dir, "%s-%s.orig" % (package_name, version)))


def make_autobuild_version(time_now):
    return "1.19.svn.%s" % time.strftime("%Y-%m-%d-%H%M", time_now)


rfc2822_format = "%a, %d %b %Y %H:%M:%S %z" # I think


class DebianVersions(object):

    def __init__(self, base_dir, next_version, time_now):
        self._version_file = os.path.join(base_dir, "current-version")
        self._time_file = os.path.join(base_dir, "current-version-time")
        self._next_version = next_version
        self._time_now = time_now

    def bump_to_next_version(self, log):
        write_file(self._version_file, self._next_version)
        write_file(self._time_file, time.strftime(rfc2822_format,
                                                  self._time_now))

    def get_version(self):
        return read_file(self._version_file)

    def make_changelog(self, package_name, variant):
        deb_version = "%s-1%s1" % (self.get_version(), variant)
        return ("""\
%(package_name)s (%(deb_version)s) %(variant)s; urgency=low

  * Automatically built source package.

 -- %(name)s <%(email)s>  %(time)s
""" %
                {"package_name": package_name,
                 "name": os.environ["DEBFULLNAME"],
                 "email": os.environ["DEBEMAIL"],
                 "time": read_file(self._time_file),
                 "variant": variant,
                 "deb_version": deb_version})


class CombinedRepo(LogDirMixin):

    def __init__(self, config, targets, dest_dir, base_dir, deb_versions):
        self._config = config
        self._targets = targets
        self._dest_dir = dest_dir
        self._base_dir = base_dir
        self._variants_dir = os.path.join(base_dir, "tmp-pkg-repos")
        self._deb_versions = deb_versions
        self._full_names = {"edgy": "ubuntu-edgy",
                            "feisty": "ubuntu-feisty",
                            "gutsy": "ubuntu-gutsy",
                            "hardy": "ubuntu-hardy",
                            "intrepid": "ubuntu-intrepid",
                            "etch": "debian-etch",
                            "lenny": "debian-lenny",
                            "sid": "debian-sid"}

    def get_name(self):
        return "combined-repo"

    def get_log_set(self):
        return build_log.LogSetDir(
            os.path.join(self._base_dir, "combined-repo-logs"))

    def make_dirs(self):
        pass

    def _get_variants(self):
        return sorted(set(target.dist for target in self._targets))

    # TODO: Remove this, it is misnamed.  Invoke deb_versions directly instead.
    def update_changelogs(self, log):
        self._deb_versions.bump_to_next_version(log)

    def get_variant_dir(self, variant):
        return os.path.join(self._variants_dir, variant)

    def reset_variants_dir(self, log):
        reset_dir(self._variants_dir)

    def make_source_packages(self, log):
        packages = [("chroot-jail", "plash-chroot-jail"),
                    ("plash", "plash")]
        for source_subdir, package_name in packages:
            make_source_packages(
                self._config, source_subdir, package_name,
                self._deb_versions.get_version(),
                [(self.get_variant_dir(variant),
                  self._deb_versions.make_changelog(package_name, variant))
                 for variant in self._get_variants()],
                self._variants_dir)

    def check_tests_passed(self, log):
        for target in self._targets:
            assert target.tests_passed(), target.get_name()

    def combine_source_and_binary_packages(self, log):
        version = self._deb_versions.get_version()
        for target in self._targets:
            # Add plash debs
            variant_name = target.dist
            variant_dir = self.get_variant_dir(variant_name)
            for built_deb in target.get_built_debs():
                assert ("_%s-" % version) in os.path.basename(built_deb), \
                    (built_deb, version)
                run_cmd(log, ["cp", "-av", built_deb, variant_dir])
        for variant_name in self._get_variants():
            variant_dir = self.get_variant_dir(variant_name)
            # Add glibc-source-X.X debs
            copy_dir(log, os.path.join(self._base_dir, "repo-template"),
                     variant_dir)
            # Create Packages and Sources files
            index_repo(log, variant_dir, self._full_names[variant_name])
        variant_dirs = [self.get_variant_dir(variant_name)
                        for variant_name in self._get_variants()]
        self._check_no_conflicts_in_repos(variant_dirs)
        self._combine_repos(log, variant_dirs)

    @action_tree.action_node
    def make_repos(self):
        return [self.reset_variants_dir,
                self.make_source_packages,
                self.check_tests_passed,
                self.combine_source_and_binary_packages]

    def _check_no_conflicts_in_repos(self, variant_dirs):
        by_file = {}
        for dir_path in variant_dirs:
            for filename in file_list_rec(dir_path):
                full_path = os.path.join(dir_path, filename)
                by_file.setdefault(filename, []).append(full_path)
        for filename, paths in by_file.iteritems():
            if len(paths) > 1:
                hashes = [get_file_hash(path) for path in paths]
                if len(set(hashes)) > 1:
                    raise Exception("Mismatch on %r: %r" % (filename, paths))

    def _combine_repos(self, log, variant_dirs):
        temp_dir = "%s-tmp" % self._dest_dir
        reset_dir(temp_dir)
        for variant_dir in variant_dirs:
            copy_dir(log, variant_dir, temp_dir)
        move_dir_onto(temp_dir, self._dest_dir)


def get_targets(config, deb_versions):
    # Plash needs Ubuntu universe for dietlibc-dev.
    ubuntu = {"name": "ubuntu", "url": config.ubuntu_mirror,
              "script": config.ubuntu_debootstrap_script,
              "subdists": ["main", "universe"]}
    ubuntu_old = {"name": "ubuntu", "url": config.ubuntu_old_mirror,
                  "script": config.ubuntu_debootstrap_script,
                  "subdists": ["main", "universe"]}
    debian = {"name": "debian", "url": config.debian_mirror,
              "script": None,
              "subdists": ["main"]}
    cases = [
        (ubuntu_old, "edgy"),
        (ubuntu, "feisty"),
        (ubuntu, "gutsy"),
        (ubuntu, "hardy"),
        (ubuntu, "intrepid"),
        (debian, "etch"),
        (debian, "lenny"),
        (debian, "sid")]
    targets = []
    for distro, release in cases:
        for arch in ("i386", "amd64"):
            bootstrapper = Debootstrapper(distro["url"], release, arch,
                                          subdists=distro["subdists"],
                                          script=distro["script"])
            # TODO: remove story6 suffix
            name = "%s-%s-%s-story6" % (arch, distro["name"], release)
            base_dir = os.path.join(config.dest_dir, name)
            stamps = StampDir(os.path.join(base_dir, "stamps"))
            chroot_env = ChrootEnv(os.path.join(base_dir, "chroot"),
                                   stamps, config.normal_user, bootstrapper)
            targets.append((name,
                            Builder(chroot_env, base_dir, stamps, name, config,
                                    deb_versions)))
    return targets


class ChrootSet(object):

    supported_targets = [# "i386-ubuntu-edgy-story6",
                         # "amd64-ubuntu-edgy-story6",
                         "i386-ubuntu-feisty-story6",
                         "amd64-ubuntu-feisty-story6",
                         "i386-ubuntu-gutsy-story6",
                         "amd64-ubuntu-gutsy-story6",
                         "i386-ubuntu-hardy-story6",
                         "amd64-ubuntu-hardy-story6",
                         # "i386-ubuntu-intrepid-story6", # build fails
                         # "amd64-ubuntu-intrepid-story6",
                         "i386-debian-lenny-story6",
                         "amd64-debian-lenny-story6",
                         ]
    autobuild_targets = ["i386-debian-etch-story6",
                         ]
    known_failures = ["i386-debian-etch-story6", # has glibc 2.3.6
                      "i386-ubuntu-intrepid-story6", # build fails
                      "amd64-ubuntu-intrepid-story6", # glibc 2.8 issue
                      "i386-ubuntu-edgy-story6", # old autotools
                      "amd64-ubuntu-edgy-story6", # old autotools
                      ]

    def __init__(self, config):
        self._time_now = time.localtime()
        self.deb_versions = DebianVersions(base_dir=config.state_dir,
                                           next_version=self.get_next_version(),
                                           time_now=self._time_now)
        self.chroots = get_targets(config, self.deb_versions)
        self.chroots_by_name = dict(self.chroots)
        self.chroot_list = [target for name, target in self.chroots]
        self.combined = CombinedRepo(
            config,
            targets=[self.chroots_by_name[name]
                     for name in self.supported_targets],
            dest_dir=os.path.join(config.state_dir, "out-repo"),
            # Relative path needed by build_log.  TODO: fix.
            base_dir=config.state_dir,
            deb_versions=self.deb_versions)
        self._path_mapper = build_log.PathnameMapper(config.state_dir)

    def get_next_version(self):
        return make_autobuild_version(self._time_now)

    def update_log(self, html_dir):
        targets = self.chroot_list + [self.combined]
        build_log.format_logs(targets, self._path_mapper,
                              os.path.join(html_dir, "summary.html"))
        build_log.format_short_summary(targets, self._path_mapper,
                                       os.path.join(html_dir, "short.html"))
        warn_targets = [self.combined] + [
            target for target in self.chroot_list
            if target.get_name() not in self.known_failures]
        stamp_file = "warning-stamp"
        stamp_time = 0
        if os.path.exists(stamp_file):
            stamp_time = os.stat(stamp_file).st_mtime
        build_log.warn_failures(warn_targets, stamp_time)

    def main(self, args):
        targets = []
        actions = []
        do_log = True

        if args == ["--log"]:
            self.update_log(html_dir=os.getcwd())
            return

        while len(args) > 0:
            arg = args.pop(0)
            if arg == "-c":
                targets.extend(self.combined._targets)
            elif arg == "-C":
                targets.append(self.combined)
            elif arg == "--all-targets":
                targets.extend(self.chroot_list)
            elif arg == "--supported":
                targets.extend([self.chroots_by_name[name]
                                for name in self.supported_targets])
            elif arg == "--autobuild":
                targets.extend([self.chroots_by_name[name]
                                for name in self.autobuild_targets])
            elif arg == "-t":
                name = args.pop(0)
                targets.append(self.chroots_by_name[name])
            elif arg == "-a":
                action = args.pop(0)
                actions.append(action)
            elif arg == "--no-log":
                do_log = False
            else:
                raise Exception("Unrecognised argument: %r" % arg)

        if len(targets) == 0:
            print "No targets given"
        if len(actions) == 0:
            print "No actions given"
        for target in targets:
            target.make_dirs()
            if do_log:
                log = target.get_log_set().make_logger()
            else:
                log = build_log.DummyLogWriter()
            try:
                for action in actions:
                    getattr(target, action)(log)
                log.finish(0)
            except:
                log.finish(1)
                log_last_exception(log)

    @action_tree.action_node
    def all(self):
        for name in self.supported_targets:
            yield name, self.chroots_by_name[name].all_steps

    @action_tree.action_node
    def build_all(self):
        # TODO: svn up first
        yield self.deb_versions.bump_to_next_version
        yield self.combined.reset_variants_dir
        yield self.combined.make_source_packages
        for name in self.supported_targets:
            yield name, self._build(self.chroots_by_name[name])
        yield self.combined.combine_source_and_binary_packages

    def _build(self, target):
        deb_source_dir = self.combined.get_variant_dir(target.dist)
        build_dir = os.path.join(target.get_dest(), target._workdir, "plash")
        def do_set_version(log):
            target.set_changelog(self.combined.make_changelog(target.dist))
        def do_unpack(log):
            dsc_file = get_one(glob.glob(os.path.join(deb_source_dir,
                                                      "plash_*.dsc")))
            target.run_cmd_as_normal_user(
                log, ["mkdir", "-p", os.path.dirname(build_dir)])
            # dpkg-source uses the cwd as a temp directory which is not good.
            # dpkg-source appears to need absolute pathnames.
            target.run_cmd_as_normal_user(
                log, ["dpkg-source", "-x", os.path.abspath(dsc_file),
                      os.path.abspath(build_dir)])
        return action_tree.make_node([
                target.tear_down,
                ("make_dirs", lambda log: target.make_dirs()),
                target.set_up,
                target.add_local_debian_repo,
                ("unpack", do_unpack),
                ("set_version", do_set_version),
                target.install_deps,
                target.build_from_debian_source,
                target.test,
                target.run_lintian,
                ], name=None)


class BuildForRelease(ChrootSet):

    def get_next_version(self):
        return "1.19"


def main(args):
    ChrootSet(chroot_build_config.ChrootConfig()).main(args)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
