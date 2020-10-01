# "uname -r" output of the kernel to build for, the running one
# if none was specified with "--define 'kernelversion <uname -r>'"
# PLEASE: provide both (correctly) or none!!
%{!?kernelversion: %{expand: %%define kernelversion %(uname -r)}}
%{!?kdir: %{expand: %%define kdir /lib/modules/%(uname -r)/build}}
%define kernel_devel_rpm %{expand:%%(rpm -q --quiet -f %{kdir}/ 2>/dev/null &&
          rpm -q --qf '%%%%{name}\\n' -f %{kdir}/ | uniq)}
%{echo:kernelversion=%{kernelversion}
}
%{echo:kdir=%{kdir}
}
%{echo:kernel_devel_rpm=%{kernel_devel_rpm}
}

# encode - to _ to be able to include that in a package name or release "number"
%global krelver  %(echo %{kernelversion} | tr -s '-' '_')

Name: drbd-km
Summary: DRBD driver for Linux
Version: 8.4.12
Release: 1
Source: http://oss.linbit.com/%{name}/8.4/drbd-%{version}-%{release}.tar.gz
License: GPLv2+
ExclusiveOS: linux
Group: System Environment/Kernel
URL: http://www.drbd.org/
BuildRoot: %(mktemp -ud %{_tmppath}/%{name}-%{version}-%{release}-XXXXXX)
BuildRequires: gcc %{kernel_devel_rpm}

%description
DRBD mirrors a block device over the network to another machine.
Think of it as networked raid 1. It is a building block for
setting up high availability (HA) clusters.

# I choose to have the kernelversion as part of the package name!
# drbd-km is prepended...
%package %{krelver}
Summary: Kernel driver for DRBD.
Group: System Environment/Kernel
# always require a suitable userland and depmod.
Requires: drbd-utils >= 8.9.2, /sbin/depmod
# to be able to override from build scripts which flavor of kernel we are building against.
Requires: %{expand: %(echo ${DRBD_KMOD_REQUIRES:-kernel})}
# TODO: break up this generic .spec file into per distribution ones,
# and use the distribution specific naming and build conventions for kernel modules.

%description %{krelver}
This module is the kernel-dependent driver for DRBD.  This is split out so
that multiple kernel driver versions can be installed, one for each
installed kernel.

%files %{krelver}
%defattr(-,root,root)
/lib/modules/%{kernelversion}/
%doc COPYING
%doc ChangeLog
%doc drbd/k-config-%{kernelversion}.gz

%prep
%setup -q -n drbd-%{version}-%{release}
test -d %{kdir}/.
test "$(KDIR=%{kdir} scripts/get_uts_release.sh)" = %{kernelversion}

%build
echo kernelversion=%{kernelversion}
echo kversion=%{kversion}
echo krelver=%{krelver}
make %{_smp_mflags} module KDIR=%{kdir}


%install
rm -rf %{buildroot}
make install DESTDIR=%{buildroot}
cd drbd
mv .kernel.config.gz k-config-%{kernelversion}.gz

%clean
rm -rf %{buildroot}

%preun %{krelver}
lsmod | grep drbd > /dev/null 2>&1
if [ $? -eq 0 ]; then
    rmmod drbd
fi

%post %{krelver}
# hack for distribution kernel packages,
# which already contain some (probably outdated) drbd module
EXTRA_DRBD_KO=/lib/modules/%{kernelversion}/extra/drbd.ko
if test -e $EXTRA_DRBD_KO; then
    mv $EXTRA_DRBD_KO $EXTRA_DRBD_KO.orig
fi
uname -r | grep BOOT ||
/sbin/depmod -a -F /boot/System.map-%{kernelversion} %{kernelversion} >/dev/null 2>&1 || true

%postun %{krelver}
/sbin/depmod -a -F /boot/System.map-%{kernelversion} %{kernelversion} >/dev/null 2>&1 || true


%changelog
* Thu Oct 01 2020 Roland Kammerer <roland.kammerer@linbit.com> - 8.4.12-1
- New upstream release.

* Thu Apr 26 2018 Lars Ellenberg <lars@linbit.com> - 8.4.11-1
- New upstream release.

* Thu Jun  1 2017 Philipp Reisner <phil@linbit.com> - 8.4.10-1
- New upstream release.

* Thu Nov 10 2016 Lars Ellenberg <lars@linbit.com> - 8.4.9-2
- Fix kernel_sendmsg() usage - potential NULL deref
  Relevant for kernel >= 4.0

* Fri Oct 21 2016 Philipp Reisner <phil@linbit.com> - 8.4.9-1
- New upstream release.

* Mon Jul 18 2016 Lars Ellenberg <lars@linbit.com> - 8.4.8-1
- New upstream release.

* Wed Dec 16 2015 Philipp Reisner <phil@linbit.com> - 8.4.7-1
- New upstream release.

* Wed Sep 16 2015 Lars Ellenberg <lars@linbit.com> - 8.4.6-5
- New upstream release.

* Thu Jul 30 2015 Lars Ellenberg <lars@linbit.com> - 8.4.6-4
- New upstream release.

* Fri Apr  3 2015 Philipp Reisner <phil@linbit.com> - 8.4.6-1
- New upstream release.

* Mon Jun  2 2014 Philipp Reisner <phil@linbit.com> - 8.4.5-1
- New upstream release.

* Fri Oct 11 2013 Philipp Reisner <phil@linbit.com> - 8.4.4-1
- New upstream release.

* Tue Feb  5 2013 Philipp Reisner <phil@linbit.com> - 8.4.3-1
- New upstream release.

* Thu Sep  6 2012 Philipp Reisner <phil@linbit.com> - 8.4.2-1
- New upstream release.

* Tue Dec 20 2011 Philipp Reisner <phil@linbit.com> - 8.4.1-1
- New upstream release.

* Mon Jul 18 2011 Philipp Reisner <phil@linbit.com> - 8.4.0-1
- New upstream release.

* Fri Jan 28 2011 Philipp Reisner <phil@linbit.com> - 8.3.10-1
- New upstream release.

* Fri Oct 22 2010 Philipp Reisner <phil@linbit.com> - 8.3.9-1
- New upstream release.

