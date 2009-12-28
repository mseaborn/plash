
# Copy this file to chroot_build_config.py and edit it.

import os

top_dir = os.path.dirname(__file__)

class ChrootConfig(object):

    state_dir = top_dir
    dest_dir = os.path.join(top_dir, "dest")

    work_tree = os.path.join(top_dir, "work")
    glibc_work_tree = os.path.join(top_dir, "glibc")

    # SVN working dir and Git repository/working dir.
    # These can be symlinks.
    plash_svn_template = os.path.join(top_dir, "master/plash")
    glibc_git_repo = os.path.join(top_dir, "master/glibc")

    ubuntu_mirror = "http://gb.archive.ubuntu.com/ubuntu"
    ubuntu_old_mirror = "http://old-releases.ubuntu.com/ubuntu"
    debian_mirror = "http://ftp.uk.debian.org/debian"
    # ubuntu_mirror = "http://localhost:9999/ubuntu"
    # ubuntu_old_mirror = "http://localhost:9999/ubuntu-old"
    # debian_mirror = "http://localhost:9999/debian"

    plash_debian_repo = "deb http://localhost/debian-repo unstable/"
    ubuntu_debootstrap_script = "debootstrap/scripts/dapper"

    # TODO: get this from SUDO_USER automatically
    normal_user = "mrs"

    enable_svn = True
