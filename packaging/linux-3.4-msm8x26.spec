%define BASE_DEF tizen_b3
%define VARI_DEF ""
%define BOARDS B3_GSM_EUR

Name: linux-3.4-msm8x26
Summary: The Linux Kernel
Version: Tizen_W3G_20140926_1_348c32e
Release: 1
License: GPL
Group: System Environment/Kernel
Vendor: The Linux Community
URL: http://www.kernel.org
Source0: %{name}-%{version}.tar.gz

BuildRoot: %{_tmppath}/%{name}-%{PACKAGE_VERSION}-root
Provides: linux-3.4
%define __spec_install_post /usr/lib/rpm/brp-compress || :
%define debug_package %{nil}

BuildRequires:  lzop
BuildRequires:  binutils-devel
BuildRequires:  module-init-tools elfutils-devel
BuildRequires:	python
BuildRequires:	gcc
BuildRequires:	bash
BuildRequires:	system-tools
BuildRequires:	sec-product-features
ExclusiveArch:  %arm

%description
The Linux Kernel, the operating system core itself

%if "%{?sec_product_feature_kernel_defconfig}" == "B3_GSM_EUR"
%define BASE_DEF tizen_b3
%define VARI_DEF ""
%define BOARDS %{sec_product_feature_kernel_defconfig}
%endif

%if "%{?sec_product_feature_kernel_defconfig}" == "B3_GSM_TMB"
%define BASE_DEF tizen_b3
%define VARI_DEF tmo
%define BOARDS %{sec_product_feature_kernel_defconfig}
%endif

%if "%{?sec_product_feature_kernel_defconfig}" == "B3_GSM_ATT"
%define BASE_DEF tizen_b3
%define VARI_DEF att
%define BOARDS %{sec_product_feature_kernel_defconfig}
%endif

%if "%{?sec_product_feature_kernel_defconfig}" == "B3_GSM_VF"
%define BASE_DEF tizen_b3
%define VARI_DEF ""
%define BOARDS %{sec_product_feature_kernel_defconfig}
%endif

%if "%{?sec_product_feature_kernel_defconfig}" == "B3_GSM_VMC"
%define BASE_DEF tizen_b3
%define VARI_DEF vmc
%define BOARDS %{sec_product_feature_kernel_defconfig}
%endif

%if "%{?sec_product_feature_kernel_defconfig}" == "B3_GSM_DCM"
%define BASE_DEF tizen_b3
%define VARI_DEF ""
%define BOARDS %{sec_product_feature_kernel_defconfig}
%endif

%if "%{?sec_product_feature_kernel_defconfig}" == "B3_CDMA_USCC"
%define BASE_DEF tizen_b3_cdma
%define VARI_DEF usc
%define BOARDS %{sec_product_feature_kernel_defconfig}
%endif

%if "%{?sec_product_feature_kernel_defconfig}" == "B3_CDMA_SPR"
%define BASE_DEF tizen_b3_cdma
%define VARI_DEF spr
%define BOARDS %{sec_product_feature_kernel_defconfig}
%endif

%if "%{?sec_product_feature_kernel_defconfig}" == "B3_CDMA_VZW"
%define BASE_DEF tizen_b3_cdma
%define VARI_DEF vzw
%define BOARDS %{sec_product_feature_kernel_defconfig}
%endif

%if "%{?sec_product_feature_kernel_defconfig}" == "B3_CDMA_KDDI"
%define BASE_DEF tizen_b3_cdma
%define VARI_DEF kdi
%define BOARDS %{sec_product_feature_kernel_defconfig}
%endif

%if "%{?sec_product_feature_kernel_defconfig}" == "B3_CDMA_LUC"
%define BASE_DEF tizen_b3_cdma
%define VARI_DEF ""
%define BOARDS %{sec_product_feature_kernel_defconfig}
%endif

%{lua:
for targets in string.gmatch(rpm.expand("%{BOARDS}"), "[%w_-]+")
do
print("%package -n linux-3.4-msm8x26_"..targets.." \n")
print("License:        TO_BE_FILLED \n")
print("Summary:        Linux support headers for userspace development \n")
print("Group:          TO_BE_FILLED/TO_BE_FILLED \n")
print("Requires(post): coreutils \n")
print("\n")
print("%files -n linux-3.4-msm8x26_"..targets.." \n")
print("/var/tmp/kernel/mod_"..targets.." \n")
print("/var/tmp/kernel/kernel-"..targets.."/dzImage \n")
print("/var/tmp/kernel/kernel-"..targets.."/dzImage-recovery \n")
print("\n")
print("%post -n linux-3.4-msm8x26_"..targets.." \n")
print("mv /var/tmp/kernel/mod_"..targets.."/lib/modules/* /lib/modules/. \n")
print("mv /var/tmp/kernel/kernel-"..targets.."/dzImage /var/tmp/kernel/. \n")
print("mv /var/tmp/kernel/kernel-"..targets.."/dzImage-recovery /var/tmp/kernel/. \n")
print("\n")
print("%description -n linux-3.4-msm8x26_"..targets.." \n")
print("This package provides the msm8x26_eur linux kernel image & module.img. \n")
print("\n")
print("%package -n linux-3.4-msm8x26_"..targets.."-debuginfo \n")
print("License:        TO_BE_FILLED \n")
print("Summary:        Linux support headers for userspace development \n")
print("Group:          TO_BE_FILLED/TO_BE_FILLED \n")
print("\n")
print("%files -n linux-3.4-msm8x26_"..targets.."-debuginfo \n")
print("/var/tmp/kernel/mod_"..targets.." \n")
print("/var/tmp/kernel/kernel-"..targets.." \n")
print("\n")
print("%description -n linux-3.4-msm8x26_"..targets.."-debuginfo \n")
print("This package provides the msm8x26_eur linux kernel's debugging files. \n")
end }

%package -n kernel-headers-3.4-msm8x26
License:        TO_BE_FILLED
Summary:        Linux support headers for userspace development
Group:          TO_BE_FILLED/TO_BE_FILLED
Provides:       kernel-headers, kernel-headers-tizen-dev
Obsoletes:      kernel-headers

%description -n kernel-headers-3.4-msm8x26
This package provides userspaces headers from the Linux kernel.  These
headers are used by the installed headers for GNU glibc and other system
 libraries.

%package -n linux-kernel-license
License:        GPL
Summary:        Linux support kernel license file
Group:          System/Kernel

%description -n linux-kernel-license
This package provides kernel license file.

%package -n kernel-devel-3.4-msm8x26
License:        GPL
Summary:        Linux support kernel map and etc for other package
Group:          System/Kernel
Provides:       kernel-devel-tizen-dev

%description -n kernel-devel-3.4-msm8x26
This package provides kernel map and etc information.

%prep
%setup -q

%build
%if 0%{?tizen_build_binary_release_type_eng}
%define RELEASE_TYPE ENG
%else
%define RELEASE_TYPE USR
%endif

for i in %{BOARDS}; do
	mkdir -p %_builddir/mod_$i
	make distclean

	./release_obs.sh %{RELEASE_TYPE} %{BASE_DEF} %{VARI_DEF}

	cp -f arch/arm/boot/merged-dtb %_builddir/merged-dtb.$i
	cp -f arch/arm/boot/zImage %_builddir/zImage.$i
	cp -f arch/arm/boot/dzImage %_builddir/dzImage.$i
	cp -f arch/arm/boot/dzImage %_builddir/dzImage-recovery.$i
	cp -f System.map %_builddir/System.map.$i
	cp -f .config %_builddir/config.$i
	cp -f vmlinux %_builddir/vmlinux.$i

	make modules
	make modules_install INSTALL_MOD_PATH=%_builddir/mod_$i

	#remove all changed source codes for next build
	cd %_builddir
	rm -rf %{name}-%{version}
	/bin/tar -xf %{SOURCE0}
	cd %{name}-%{version}
done

%install
mkdir -p %{buildroot}/usr
make mrproper
make headers_check
make headers_install INSTALL_HDR_PATH=%{buildroot}/usr

find  %{buildroot}/usr/include -name ".install" | xargs rm -f
find  %{buildroot}/usr/include -name "..install.cmd" | xargs rm -f
rm -rf %{buildroot}/usr/include/scsi
rm -f %{buildroot}/usr/include/asm*/atomic.h
rm -f %{buildroot}/usr/include/asm*/io.h

mkdir -p %{buildroot}/usr/share/license
cp -vf COPYING %{buildroot}/usr/share/license/linux-kernel

for i in %{BOARDS}; do
	mkdir -p %{buildroot}/var/tmp/kernel/kernel-$i
	mkdir -p %{buildroot}/var/tmp/kernel/license-$i

	mv %_builddir/mod_$i %{buildroot}/var/tmp/kernel/mod_$i

	mv %_builddir/merged-dtb.$i %{buildroot}/var/tmp/kernel/kernel-$i/merged-dtb
	mv %_builddir/zImage.$i %{buildroot}/var/tmp/kernel/kernel-$i/zImage
	mv %_builddir/dzImage.$i %{buildroot}/var/tmp/kernel/kernel-$i/dzImage
	mv %_builddir/dzImage-recovery.$i %{buildroot}/var/tmp/kernel/kernel-$i/dzImage-recovery

	mv %_builddir/System.map.$i %{buildroot}/var/tmp/kernel/kernel-$i/System.map
	mv %_builddir/config.$i %{buildroot}/var/tmp/kernel/kernel-$i/config
	mv %_builddir/vmlinux.$i %{buildroot}/var/tmp/kernel/kernel-$i/vmlinux
done

find %{buildroot}/var/tmp/kernel/ -name 'System.map' > develfiles.pre # for secure storage
find %{buildroot}/var/tmp/kernel/ -name 'vmlinux' >> develfiles.pre   # for TIMA
find %{buildroot}/var/tmp/kernel/ -name '*.ko' >> develfiles.pre      # for TIMA
cat develfiles.pre | sed -e "s#%{buildroot}##g" | uniq | sort > develfiles

%clean
rm -rf %_builddir

%files -n kernel-headers-3.4-msm8x26
/usr/include/*

%files -n linux-kernel-license
/usr/share/license/*

%files -n kernel-devel-3.4-msm8x26 -f develfiles
