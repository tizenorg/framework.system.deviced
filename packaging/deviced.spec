
# display, extcon, stroage, power, usb are always enable
%define battery_module on
%define block_module on
# What use is bluetooth module ?
%define bluetooth_module off
# Is it profile feature ?
%define display_module on
%define extcon_module on
%define haptic_module on
%define hdmi_cec_module on
%define led_module on
# Will be used before long
%define pass_module off
%define pmqos_module on
%define power_module on
%define powersaver_module off
%define storage_module on
%define telephony_module on
%define touch_module on
%define tzip_module on
%define usb_module on
%define usbhost_module on
#Just For debugging
%define sdb_prestart on

%if "%{?tizen_profile_name}" == "mobile"
%define bluetooth_module on
%define sdb_prestart on
%endif
%if "%{?tizen_profile_name}" == "wearable"
%define block_module off
%define hdmi_cec_module off
%define led_module off
%define usbhost_module off
%define sdb_prestart off
%endif
%if "%{?tizen_profile_name}" == "tv"
%define battery_module off
%define haptic_module off
%define hdmi_cec_module off
%define led_module off
%define pmqos_module off
%define telephony_module off
%define touch_module off
%define tzip_module off
%define usb_module off
%define sdb_prestart off
%endif

Name:       deviced
Summary:    Deviced
Version:    1.0.1
Release:    1
Group:      Framework/system
License:    Apache-2.0 and BSD 2-clause and BSD-1.0
Source0:    %{name}-%{version}.tar.gz
Source1:    deviced.manifest
Source2:    libdeviced.manifest
Source3:    deviced-tools.manifest

BuildRequires:  cmake
BuildRequires:  libattr-devel
BuildRequires:  gettext-devel
BuildRequires:  pkgconfig(ecore)
BuildRequires:  pkgconfig(eina)
BuildRequires:  pkgconfig(vconf)
BuildRequires:  pkgconfig(dlog)
BuildRequires:  pkgconfig(device-node)
BuildRequires:  pkgconfig(edbus)
BuildRequires:  pkgconfig(capi-base-common)
BuildRequires:  pkgconfig(libudev)
BuildRequires:  pkgconfig(gio-2.0)
BuildRequires:	pkgconfig(eventsystem)
%if %{?display_module} == on
BuildRequires:	pkgconfig(libinput)
BuildRequires:	pkgconfig(capi-system-sensor)
%endif
%if %{?storage_module} == on
BuildRequires:	pkgconfig(storage)
%endif
%if %{?telephony_module} == on
BuildRequires:	pkgconfig(tapi)
%endif
%if %{?tzip_module} == on
BuildRequires:	pkgconfig(fuse)
BuildRequires:	pkgconfig(minizip)
%endif

Requires: %{name}-tools = %{version}-%{release}
%{?systemd_requires}
Requires(post): /usr/bin/vconftool
Requires(post): pulseaudio

%description
deviced

%package deviced
Summary:    deviced daemon
Group:      main

%description deviced
deviced daemon.

%package tools
Summary:  Deviced tools
Group:    System/Utilities

%description tools
Deviced helper programs

%package -n libdeviced
Summary:    Deviced library
Group:      Development/Libraries
Requires:   %{name} = %{version}-%{release}

%description -n libdeviced
Deviced library for device control

%package -n libdeviced-devel
Summary:    Deviced library for (devel)
Group:      Development/Libraries
Requires:   libdeviced = %{version}-%{release}

%description -n libdeviced-devel
Deviced library for device control (devel)

%prep
%setup -q

export CFLAGS+=" -DTIZEN_DEBUG_ENABLE"
export CFLAGS+=" -DTIZEN_ENGINEER_MODE"
%define ENGINEER_MODE 1

%ifarch %{arm}
%define ARCH arm
%else
%define ARCH emulator
%endif
cmake . \
	-DCMAKE_INSTALL_PREFIX=%{_prefix} \
	-DARCH=%{ARCH} \
	-DPROFILE=%{tizen_profile_name} \
	-DBATTERY_MODULE=%{battery_module} \
	-DBLOCK_MODULE=%{block_module} \
	-DBLUETOOTH_MODULE=%{bluetooth_module} \
	-DDISPLAY_MODULE=%{display_module} \
	-DEXTCON_MODULE=%{extcon_module} \
	-DHAPTIC_MODULE=%{haptic_module} \
	-DHDMI_CEC_MODULE=%{hdmi_cec_module} \
	-DLED_MODULE=%{led_module} \
	-DPASS_MODULE=%{pass_module} \
	-DPMQOS_MODULE=%{pmqos_module} \
	-DPOWER_MODULE=%{power_module} \
	-DPOWERSAVER_MODULE=%{powersaver_module} \
	-DSTORAGE_MODULE=%{storage_module} \
	-DTELEPHONY_MODULE=%{telephony_module} \
	-DTOUCH_MODULE=%{touch_module} \
	-DTZIP_MODULE=%{tzip_module} \
	-DUSB_MODULE=%{usb_module} \
	-DUSBHOST_MODULE=%{usbhost_module}

%build
cp %{SOURCE1} .
cp %{SOURCE2} .
cp %{SOURCE3} .
make

%install
rm -rf %{buildroot}
%make_install

mkdir -p %{buildroot}%{_unitdir}/multi-user.target.wants
ln -s ../deviced.service %{buildroot}%{_unitdir}/multi-user.target.wants/deviced.service
# Temporary symlink
ln -s deviced.service %{buildroot}%{_unitdir}/system-server.service
ln -s ../early-booting-done.service %{buildroot}%{_unitdir}/multi-user.target.wants/early-booting-done.service

mkdir -p %{buildroot}%{_unitdir}/graphical.target.wants
ln -s ../zbooting-done.service %{buildroot}%{_unitdir}/graphical.target.wants/zbooting-done.service

mkdir -p %{buildroot}%{_unitdir}/shutdown.target.wants
ln -s ../shutdown-notify.service %{buildroot}%{_unitdir}/shutdown.target.wants/shutdown-notify.service
ln -s ../devicectl-stop@.service %{buildroot}%{_unitdir}/shutdown.target.wants/devicectl-stop@display.service

mkdir -p %{buildroot}%{_datadir}/dbus-1/system-services/
install -m 0644 systemd/org.tizen.system.deviced.service %{buildroot}%{_datadir}/dbus-1/system-services/

mkdir -p %{buildroot}%{_sysconfdir}/systemd/default-extra-dependencies/ignore-units.d/
ln -s %{_unitdir}/deviced.service %{buildroot}%{_sysconfdir}/systemd/default-extra-dependencies/ignore-units.d/

%if %{?sdb_prestart} == on
mkdir -p %{buildroot}%{_unitdir}/basic.target.wants
install -m 0644 systemd/sdb-prestart.service %{buildroot}%{_unitdir}/sdb-prestart.service
ln -s ../sdb-prestart.service %{buildroot}%{_unitdir}/basic.target.wants/sdb-prestart.service
%endif

mkdir -p %{buildroot}%{_datadir}/license
cp LICENSE %{buildroot}/usr/share/license/%{name}
cp LICENSE %{buildroot}/usr/share/license/libdeviced

%post
%if %{?display_module} == on
#db type vconf key init
vconftool set -t int db/private/sysman/enhance_mode 0 -i -s tizen::vconf::platform::r # used in enhance-mobile
vconftool set -t int db/private/sysman/enhance_scenario 0 -i -s tizen::vconf::platform::r # used in enhance-mobile
vconftool set -t bool db/private/deviced/lcd_brightness_init 0 -i -s tizen::vconf::platform::r # used in display-dbus
%endif

%if %{?usb_module} == on
#USB client
vconftool set -t int db/private/usb/usb_control "1" -u 0 -i -s tizen::vconf::platform::r # used in usb-client-control
%endif

systemctl daemon-reload
if [ $1 == 1 ]; then
    systemctl restart deviced.service
    systemctl restart early-booting-done.service
    systemctl restart zbooting-done.service
fi

%preun
if [ $1 == 0 ]; then
    systemctl stop deviced.service
    systemctl stop zbooting-done.service
fi

%postun
systemctl daemon-reload

%files -n deviced
%manifest %{name}.manifest
%{_datadir}/license/%{name}
%{_bindir}/deviced
%{_datadir}/dbus-1/system-services/org.tizen.system.deviced.service
%{_unitdir}/deviced.service
%{_unitdir}/multi-user.target.wants/deviced.service
%{_sysconfdir}/systemd/default-extra-dependencies/ignore-units.d/deviced.service
# Temporary symlink service
%{_unitdir}/system-server.service
%{_unitdir}/zbooting-done.service
%{_unitdir}/early-booting-done.service
%{_unitdir}/graphical.target.wants/zbooting-done.service
%{_unitdir}/multi-user.target.wants/early-booting-done.service
%{_unitdir}/shutdown-notify.service
%{_unitdir}/shutdown.target.wants/shutdown-notify.service
%{_unitdir}/shutdown.target.wants/devicectl-stop@display.service
%if %{?sdb_prestart} == on
%{_unitdir}/sdb-prestart.service
%{_unitdir}/basic.target.wants/sdb-prestart.service
%endif
%{_sysconfdir}/smack/accesses.d/deviced.efl
%if "%{?tizen_profile_name}" == "wearable"
%else
%{_bindir}/movi_format.sh
%endif
%if %{?battery_module} == on
%config %{_sysconfdir}/deviced/battery.conf
%endif
%if %{?block_module} == on
%{_bindir}/mmc-smack-label
%{_bindir}/fsck_msdosfs
%{_bindir}/newfs_msdos
%{_datadir}/license/fsck_msdosfs
%{_datadir}/license/newfs_msdos
%config %{_sysconfdir}/deviced/block.conf
%config %{_sysconfdir}/deviced/mmc.conf
%endif
%if %{?display_module} == on
%config %{_sysconfdir}/deviced/display.conf
%endif
%if %{?haptic_module} == on
%config %{_sysconfdir}/deviced/haptic.conf
%endif
%if %{?led_module} == on
%config %{_sysconfdir}/deviced/led.conf
%endif
%if %{?pass_module} == on
%config %{_sysconfdir}/deviced/pass.conf
%endif
%if %{?pmqos_module} == on
%config %{_sysconfdir}/deviced/pmqos.conf
%endif
%if %{?power_module} == on
%config %{_sysconfdir}/deviced/power.conf
%endif
%if %{?storage_module} == on
%config %{_sysconfdir}/deviced/storage.conf
%endif
%if %{?usb_module} == on
%config %{_sysconfdir}/deviced/usb-client-configuration.conf
%config %{_sysconfdir}/deviced/usb-client-operation.conf
%endif

%files tools
%manifest deviced-tools.manifest
%{_bindir}/devicectl
%{_bindir}/deviced-auto-test
%{_datadir}/dbus-1/system-services/org.tizen.system.deviced-auto-test.service
%{_unitdir}/deviced-auto-test.service
%{_unitdir}/devicectl-stop@.service
%{_unitdir}/devicectl-start@.service
%if %{?usb_module} == on
%if %ENGINEER_MODE
%attr(750,root,root) %{_bindir}/set_usb_debug.sh
%attr(750,root,root) %{_bindir}/direct_set_debug.sh
%endif
%endif

%files -n libdeviced
%manifest libdeviced.manifest
%{_datadir}/license/libdeviced
%defattr(-,root,root,-)
%{_libdir}/libdeviced.so.*

%files -n libdeviced-devel
%defattr(-,root,root,-)
%{_includedir}/deviced/*.h
%{_libdir}/libdeviced.so
%{_libdir}/pkgconfig/deviced.pc
