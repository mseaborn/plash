# Please note that this spec file is a hack!
# You can't use it on its own to build from source
# -- it is intended for packaging binaries that have already been built.
# See rpm/make-rpm.sh.
Summary: Sandbox programs to run them with minimum authority
Name: plash
Version: 1.16
Release: 1
Packager: Mark Seaborn <mrs@mythic-beasts.com>
Copyright: LGPL
Source0: %{name}-%{version}.tar.gz
Group: System Environment
BuildRoot: %{_tmppath}/%{name}-root
# This stops RPM from looking at all the lib*.so parts of libc.  They
# depend on each other, not on any outside packages.
# But how am I supposed to include the output of
# "echo /usr/bin/plash /usr/lib/rpm/find-requires" in the dependency info?
AutoReqProv: no

%description

Plash is a sandbox for running GNU/Linux programs with the minimum
necessary privileges. It is similar to chroot jails, but is more
lightweight and flexible. You can use Plash to grant a process
read-only or read-write access to specific files and directories,
which can be mapped at any point in its private filesystem namespace.

Plash provides a command line tool (pola-run) for running programs in
a sandbox, granting access to specific files and directories.

Plash also provides a "powerbox" user interface by which the user can
grant an application the right to access files.  A powerbox is just
like a normal file chooser dialog box, except that it also grants
access rights.  The powerbox is implemented as a trusted component --
applications must ask the system to open a file chooser, rather than
implementing it themselves.  Plash comes with a patch to Gtk to
implement GtkFileChooserDialog in terms of the powerbox API.

The Plash execution environment doesn't require a modified Linux
kernel -- it uses chroot() and UIDs. It works with existing Linux
executables, provided they are dynamically linked, because Plash uses
a modified version of GNU libc.

Plash virtualizes the filesystem. With the modified libc, open() works
by sending a request across a socket. The server process can send a
file descriptor back across the socket in response. Usually, Plash
does not slow programs down because the most frequently used system
calls (such as read() and write()) work on kernel-level file
descriptors as before.


%prep
%setup -q

%build

%install

# Could do "rm -rv %{buildroot}" at the start, but that seems really dangerous

mkdir -p %{buildroot}/usr/share/doc/plash-%{version}
mkdir -p %{buildroot}/usr/share/doc/plash-%{version}/html
mkdir -p %{buildroot}/usr/share/man/man1

# Install docs
# Removed -p option
cp -L -rv plash/COPYRIGHT \
	plash/debian/changelog \
	%{buildroot}/usr/share/doc/plash-%{version}/
cp -rv plash/web-site/out \
	%{buildroot}/usr/share/doc/plash-%{version}/html

# Install man pages
cp -v plash/docs/man/* %{buildroot}/usr/share/man/man1/
gzip -9 %{buildroot}/usr/share/man/man1/*.1
( cd %{buildroot}/usr/share/man/man1 && \
  ln -s plash-opts.1.gz plash-opts-gtk.1.gz ) || false


( cd plash && ./install.sh --nocheckroot %{buildroot} ) || false


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
