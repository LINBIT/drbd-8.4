Name: drbd-kernel
Summary: Kernel driver for DRBD
Version: 8.4.12
Release: 1%{?dist}
%global tarball_version %(echo "%{version}-%{?release}" | sed -e "s,%{?dist}$,,")
Source: http://oss.linbit.com/drbd/drbd-%{tarball_version}.tar.gz
License: GPLv2+
Group: System Environment/Kernel
URL: http://www.drbd.org/
BuildRoot: %(mktemp -ud %{_tmppath}/%{name}-%{version}-%{release}-XXXXXX)
%if ! %{defined suse_version}
BuildRequires: redhat-rpm-config
%endif
%if %{defined kernel_module_package_buildreqs}
BuildRequires: %kernel_module_package_buildreqs
%endif

%description
This module is the kernel-dependent driver for DRBD.  This is split out so
that multiple kernel driver versions can be installed, one for each
installed kernel.

%prep
%setup -q -n drbd-%{tarball_version}

%if %{defined suse_kernel_module_package}
# Support also sles10, where kernel_module_package was not yet defined.
# In sles11, suse_k_m_p became a wrapper around k_m_p.

%if 0%{?suse_version} < 1110
# We need to exclude some flavours on sles10 etc,
# or we hit an rpm internal buffer limit.
%suse_kernel_module_package -n drbd -p preamble -f filelist-suse kdump kdumppae vmi vmipae um
%else
%suse_kernel_module_package -n drbd -p preamble -f filelist-suse
%endif
%else
# Concept stolen from sles kernel-module-subpackage:
# include the kernel version in the package version,
# so we can have more than one kmod-drbd.
# Needed, because even though kABI is still "compatible" in RHEL 6.0 to 6.1,
# the actual functionality differs very much: 6.1 does no longer do BARRIERS,
# but wants FLUSH/FUA instead.
# For convenience, we want both 6.0 and 6.1 in the same repository,
# and have yum/rpm figure out via dependencies, which kmod version should be installed.
# This is a dirty hack, non generic, and should probably be enclosed in some "if-on-rhel6".
%define _this_kmp_version %{version}_%(echo %kernel_version | sed -r 'y/-/_/; s/\.el.\.(x86_64|i.86)$//;')
%kernel_module_package -v %_this_kmp_version -n drbd -p preamble -f filelist-redhat
%endif

%build
rm -rf obj
mkdir obj
ln -s ../scripts obj/

for flavor in %flavors_to_build; do
    cp -r drbd obj/$flavor
    #make -C %{kernel_source $flavor} M=$PWD/obj/$flavor
    make -C obj/$flavor %{_smp_mflags} all KDIR=%{kernel_source $flavor}
done

%install
export INSTALL_MOD_PATH=$RPM_BUILD_ROOT

%if %{defined kernel_module_package_moddir}
export INSTALL_MOD_DIR=%{kernel_module_package_moddir drbd}
%else
%if %{defined suse_kernel_module_package}
export INSTALL_MOD_DIR=updates
%else
export INSTALL_MOD_DIR=extra/drbd
%endif
%endif

# Very likely kernel_module_package_moddir did ignore the parameter,
# so we just append it here. The weak-modules magic expects that location.
[ $INSTALL_MOD_DIR = extra ] && INSTALL_MOD_DIR=extra/drbd

for flavor in %flavors_to_build ; do
    make -C %{kernel_source $flavor} modules_install \
	M=$PWD/obj/$flavor
    kernelrelease=$(cat %{kernel_source $flavor}/include/config/kernel.release || make -s -C %{kernel_source $flavor} kernelrelease)
    mv obj/$flavor/.kernel.config.gz obj/k-config-$kernelrelease.gz
done

%if %{defined suse_kernel_module_package}
# On SUSE, putting the modules into the default path determined by
# %kernel_module_package_moddir is enough to give them priority over
# shipped modules.
rm -f drbd.conf
%else
mkdir -p $RPM_BUILD_ROOT/etc/depmod.d
echo "override drbd * weak-updates" \
    > $RPM_BUILD_ROOT/etc/depmod.d/drbd.conf
install -D misc/SECURE-BOOT-KEY-linbit.com.der $RPM_BUILD_ROOT/etc/pki/linbit/SECURE-BOOT-KEY-linbit.com.der
%endif

%clean
rm -rf %{buildroot}

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

* Thu Nov 25 2010 Andreas Gruenbacher <agruen@linbit.com> - 8.3.9-1
- Convert to a Kernel Module Package.
