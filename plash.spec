Summary: Principle of Least Authority shell (Plash)
Name: plash
Version: 1.11
Release: 1
Packager: Mark Seaborn <mseaborn@onetel.com>
Copyright: GPL and LGPL
Source0: %{name}-%{version}.tar.gz
Group: System Environment/Shells
BuildRoot: %{_tmppath}/%{name}-root
# This stops RPM from looking at all the lib*.so parts of libc.  They
# depend on each other, not on any outside packages.
# But how am I supposed to include the output of
# "echo /usr/bin/plash /usr/lib/rpm/find-requires" in the dependency info?
AutoReqProv: no

%description

Plash (the Principle of Least Authority shell) is a replacement Unix
shell which lets the user run Linux programs with access only to the
files and directories that they need to run.

It works by virtualizing the filesystem.  Each process can have its
own file namespace.

This implemented in two steps: Firstly, processes are run in a
chroot() environment under different UIDs, so they can't access files
using the normal Linux system calls and are isolated from each other.
Secondly, in order to open files, a process makes requests to a server
process via a socket; the server can send file descriptors across the
socket in reply.

Plash dynamically links programs with a modified version of GNU libc
so that they can do filesystem operations using this different
mechanism.

No kernel modifications are required.  Plash can run Linux binaries
unmodified, provided they are dynamically linked with libc, which is
almost always the case.

In most cases this does not affect performance because the most
frequently called system calls, such as read() and write(), are not
affected.


%prep
%setup -q

%build

%install

# Could do "rm -rv %{buildroot}" at the start, but that seems really dangerous

mkdir -p %{buildroot}/usr/share/doc/plash-%{version}
mkdir -p %{buildroot}/usr/share/doc/plash-%{version}/html
mkdir -p %{buildroot}/usr/share/man/man1
mkdir -p %{buildroot}/usr/lib/plash/lib
mkdir -p %{buildroot}/var/lib/plash-chroot-jail/special
mkdir -p %{buildroot}/var/lib/plash-chroot-jail/plash-uid-locks
mkdir -p %{buildroot}/usr/bin
mkdir -p %{buildroot}/usr/share/emacs/site-lisp/plash/

# Install docs
cp -v plash/COPYRIGHT        %{buildroot}/usr/share/doc/plash-%{version}/
cp -v plash/README           %{buildroot}/usr/share/doc/plash-%{version}/
cp -v plash/NOTES            %{buildroot}/usr/share/doc/plash-%{version}/
cp -v plash/NOTES.exec       %{buildroot}/usr/share/doc/plash-%{version}/
cp -v plash/BUGS             %{buildroot}/usr/share/doc/plash-%{version}/
cp -v plash/protocols.txt    %{buildroot}/usr/share/doc/plash-%{version}/
cp -v plash/debian/changelog %{buildroot}/usr/share/doc/plash-%{version}/
cp -v plash/docs/out-html/*  %{buildroot}/usr/share/doc/plash-%{version}/html/

# Install man pages
cp -v plash/docs/out-man/plash.1 \
      plash/docs/out-man/exec-object.1 \
      plash/docs/out-man/plash-opts.1 \
      plash/docs/out-man/plash-chroot.1 \
      plash/docs/out-man/plash-run-emacs.1 \
      plash/docs/out-man/plash-socket-connect.1 \
      plash/docs/out-man/plash-socket-publish.1 \
      %{buildroot}/usr/share/man/man1/
gzip -9 %{buildroot}/usr/share/man/man1/*.1
( cd %{buildroot}/usr/share/man/man1 && ln -s plash-opts.1.gz plash-opts-gtk.1.gz )

# Install libraries
( cd plash && ./src/install.pl --dest-dir %{buildroot}/usr/lib/plash/lib/ )
strip --remove-section=.comment --remove-section=.note plash/shobj/ld.so -o %{buildroot}/var/lib/plash-chroot-jail/special/ld-linux.so.2
chmod +x %{buildroot}/var/lib/plash-chroot-jail/special/ld-linux.so.2

# Install executables
STRIP_ARGS="--remove-section=.comment --remove-section=.note"
# cp -v plash/bin/plash           %{buildroot}/usr/bin/
# cp -v plash/bin/plash-chroot    %{buildroot}/usr/bin/
# cp -v plash/bin/plash-opts      %{buildroot}/usr/bin/
# cp -v plash/bin/plash-opts-gtk  %{buildroot}/usr/bin/
# cp -v plash/bin/exec-object     %{buildroot}/usr/bin/
strip $STRIP_ARGS plash/bin/plash           -o %{buildroot}/usr/bin/plash
strip $STRIP_ARGS plash/bin/plash-chroot    -o %{buildroot}/usr/bin/plash-chroot
strip $STRIP_ARGS plash/bin/plash-opts      -o %{buildroot}/usr/bin/plash-opts
strip $STRIP_ARGS plash/bin/plash-opts-gtk  -o %{buildroot}/usr/bin/plash-opts-gtk
strip $STRIP_ARGS plash/bin/exec-object     -o %{buildroot}/usr/bin/exec-object
strip $STRIP_ARGS plash/bin/socket-connect  -o %{buildroot}/usr/bin/plash-socket-connect
strip $STRIP_ARGS plash/bin/socket-publish  -o %{buildroot}/usr/bin/plash-socket-publish
strip $STRIP_ARGS plash/bin/run-emacs       -o %{buildroot}/usr/bin/plash-run-emacs

# cp -v plash/mrs/run-as-nobody %{buildroot}/usr/lib/plash/
# cp -v plash/mrs/run-as-nobody+chroot %{buildroot}/usr/lib/plash/
cp -v plash/setuid/run-as-anonymous %{buildroot}/usr/lib/plash/
cp -v plash/setuid/gc-uid-locks %{buildroot}/usr/lib/plash/

# Install Emacs Lisp file
cp -v plash/src/plash-gnuserv.el %{buildroot}/usr/share/emacs/site-lisp/plash/

%clean

%files
# -f glibc/mrs/installed-file-list

# This has to come first, so that it applies to the directories below
%defattr(-,root,root)

/usr/bin
/usr/lib/plash
/var/lib/plash-chroot-jail
/usr/share/man
/usr/share/doc/plash-%{version}
/usr/share/emacs

# %attr(06755,root,root) /usr/lib/plash/run-as-nobody
# %attr(06755,root,root) /usr/lib/plash/run-as-nobody+chroot
%attr(06755,root,root) /usr/lib/plash/run-as-anonymous
%attr(06755,root,root) /usr/lib/plash/gc-uid-locks
%attr(00700,root,root) /var/lib/plash-chroot-jail/plash-uid-locks


%changelog
* Sun Dec  5 2004 Mark Seaborn <mseaborn@onetel.com>
- Initial build.  I'm not going to fill this changelog out...
