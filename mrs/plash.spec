Summary: Principle of Least Authority Shell (Plash)
Name: plash
Version: 1.5
Release: 1
Packager: Mark Seaborn <mseaborn@jhu.edu>
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

Plash is a replacement Unix shell which allows the user to run
programs with access only to the files that they need to run.

It works by virtualising the filesystem.  This is implemented by
running programs with a modified version of GNU libc.

%prep
%setup -q

%build

%install

install -d %{buildroot}/usr/share/doc/plash-%{version}
install -d %{buildroot}/usr/lib/plash/lib
install -d %{buildroot}/usr/lib/plash-chroot-jail/special
install -d %{buildroot}/usr/lib/plash-chroot-jail/plash-uid-locks
install -d %{buildroot}/usr/bin

cp -v glibc/mrs/COPYRIGHT %{buildroot}/usr/share/doc/plash-%{version}/
cp -v glibc/mrs/README %{buildroot}/usr/share/doc/plash-%{version}/
cp -v glibc/mrs/protocols.txt %{buildroot}/usr/share/doc/plash-%{version}/
cp -v glibc/mrs/debian/changelog %{buildroot}/usr/share/doc/plash-%{version}/
cp -v glibc/mrs/inst_stripped/lib/* %{buildroot}/usr/lib/plash/lib/
cp -v glibc/mrs/inst_stripped/ld-linux.so.2 %{buildroot}/usr/lib/plash-chroot-jail/special/
cp -v glibc/mrs/shell %{buildroot}/usr/bin/plash
cp -v glibc/mrs/shell-nogtk %{buildroot}/usr/bin/plash-nogtk
cp -v glibc/mrs/chroot %{buildroot}/usr/bin/plash-chroot
cp -v glibc/mrs/run-as-nobody %{buildroot}/usr/lib/plash/
cp -v glibc/mrs/run-as-nobody+chroot %{buildroot}/usr/lib/plash/
cp -v glibc/mrs/run-as-anonymous %{buildroot}/usr/lib/plash/
cp -v glibc/mrs/gc-uid-locks %{buildroot}/usr/lib/plash/

%clean

%files
# -f glibc/mrs/installed-file-list
/usr/bin
/usr/lib/plash
/usr/lib/plash-chroot-jail
/usr/share/doc/plash-%{version}
%defattr(-,root,root)
%attr(06755,root,root) /usr/lib/plash/run-as-nobody
%attr(06755,root,root) /usr/lib/plash/run-as-nobody+chroot
%attr(06755,root,root) /usr/lib/plash/run-as-anonymous
%attr(06755,root,root) /usr/lib/plash/gc-uid-locks


%changelog
* Sun Dec  5 2004 Mark Seaborn <mseaborn@jhu.edu>
- Initial build.
