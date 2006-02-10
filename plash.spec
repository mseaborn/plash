Summary: Principle of Least Authority shell (Plash)
Name: plash
Version: 1.12
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

# Install docs
cp -prv plash/COPYRIGHT plash/README \
	plash/docs/README.old plash/docs/NEWS plash/docs/NEWS-exec-objs \
	plash/docs/protocols.txt \
	plash/debian/changelog \
	plash/docs/html \
	%{buildroot}/usr/share/doc/plash-%{version}/

# Install man pages
cp -pv plash/docs/man/* %{buildroot}/usr/share/man/man1/
gzip -9 %{buildroot}/usr/share/man/man1/*.1
( cd %{buildroot}/usr/share/man/man1 && \
  ln -s plash-opts.1.gz plash-opts-gtk.1.gz )


( cd plash && ./install.sh %{buildroot} )


%clean

%files

# This has to come first, so that it applies to the directories below
%defattr(-,root,root)

/usr/bin
/usr/lib/plash
/var/lib/plash-chroot-jail
/usr/share/man
/usr/share/doc/plash-%{version}
/usr/share/emacs

%attr(06755,root,root) /usr/lib/plash/run-as-anonymous
%attr(06755,root,root) /usr/lib/plash/gc-uid-locks
%attr(06755,root,root) /var/lib/plash-chroot-jail/run-as-anonymous
%attr(00700,root,root) /var/lib/plash-chroot-jail/plash-uid-locks


%changelog
* Sun Dec  5 2004 Mark Seaborn <mseaborn@onetel.com>
- Initial build.  I'm not going to fill this changelog out...
