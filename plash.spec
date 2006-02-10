Summary: Principle of Least Authority shell (Plash)
Name: plash
Version: 1.8
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

install -d %{buildroot}/usr/share/doc/plash-%{version}
install -d %{buildroot}/usr/share/man/man1
install -d %{buildroot}/usr/lib/plash/lib
install -d %{buildroot}/usr/lib/plash-chroot-jail/special
install -d %{buildroot}/usr/lib/plash-chroot-jail/plash-uid-locks
install -d %{buildroot}/usr/bin

# Install docs
cp -pv plash/COPYRIGHT     %{buildroot}/usr/share/doc/plash-%{version}/
cp -pv plash/README        %{buildroot}/usr/share/doc/plash-%{version}/
cp -pv plash/NOTES         %{buildroot}/usr/share/doc/plash-%{version}/
cp -pv plash/NOTES.exec    %{buildroot}/usr/share/doc/plash-%{version}/
cp -pv plash/BUGS          %{buildroot}/usr/share/doc/plash-%{version}/
cp -pv plash/protocols.txt %{buildroot}/usr/share/doc/plash-%{version}/
cp -pv plash/debian/changelog %{buildroot}/usr/share/doc/plash-%{version}/

# Install man pages
cp -pv plash/docs/plash.1        %{buildroot}/usr/share/man/man1/
cp -pv plash/docs/exec-object.1  %{buildroot}/usr/share/man/man1/
cp -pv plash/docs/plash-opts.1   %{buildroot}/usr/share/man/man1/
cp -pv plash/docs/plash-chroot.1 %{buildroot}/usr/share/man/man1/
gzip -9 %{buildroot}/usr/share/man/man1/*.1
( cd %{buildroot}/usr/share/man/man1 && ln -s plash-opts.1.gz plash-opts-gtk.1.gz )

# Install libraries
cp -pv plash/out-stripped/lib/* %{buildroot}/usr/lib/plash/lib/
cp -pv plash/out-stripped/ld-linux.so.2 %{buildroot}/usr/lib/plash-chroot-jail/special/
chmod +x %{buildroot}/usr/lib/plash-chroot-jail/special/ld-linux.so.2

# Install executables
cp -pv plash/bin/plash           %{buildroot}/usr/bin/
cp -pv plash/bin/plash-chroot    %{buildroot}/usr/bin/
cp -pv plash/bin/plash-opts      %{buildroot}/usr/bin/
cp -pv plash/bin/plash-opts-gtk  %{buildroot}/usr/bin/
cp -pv plash/bin/exec-object     %{buildroot}/usr/bin/

# cp -pv plash/mrs/run-as-nobody %{buildroot}/usr/lib/plash/
# cp -pv plash/mrs/run-as-nobody+chroot %{buildroot}/usr/lib/plash/
cp -pv plash/setuid/run-as-anonymous %{buildroot}/usr/lib/plash/
cp -pv plash/setuid/gc-uid-locks %{buildroot}/usr/lib/plash/

%clean

%files
# -f glibc/mrs/installed-file-list

# This has to come first, so that it applies to the directories below
%defattr(-,root,root)

/usr/bin
/usr/lib/plash
/usr/lib/plash-chroot-jail
/usr/share/man
/usr/share/doc/plash-%{version}

# %attr(06755,root,root) /usr/lib/plash/run-as-nobody
# %attr(06755,root,root) /usr/lib/plash/run-as-nobody+chroot
%attr(06755,root,root) /usr/lib/plash/run-as-anonymous
%attr(06755,root,root) /usr/lib/plash/gc-uid-locks
%attr(00700,root,root) /usr/lib/plash-chroot-jail/plash-uid-locks


%changelog
* Sun Dec  5 2004 Mark Seaborn <mseaborn@onetel.com>
- Initial build.  I'm not going to fill this changelog out...
