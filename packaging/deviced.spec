Name:       deviced
Summary:    deviced
Version:    1.0.0
Release:    1
Group:      Framework/system
License:    Apache License, Version 2.0
Source0:    %{name}-%{version}.tar.gz
Source1:    %{name}.service
Source2:    zbooting-done.service
Source3:    shutdown-notify.service
Source4:    deviced-pre.service
Source5:    devicectl-start@.service
Source6:    devicectl-stop@.service
Source1001: deviced.manifest
Source1002: libdeviced.manifest
Source1003: liblogd-db.manifest
Source1004: liblogd.manifest

BuildRequires:  cmake
BuildRequires:  libattr-devel
BuildRequires:  gettext-devel
BuildRequires:  pkgconfig(ecore)
BuildRequires:  pkgconfig(vconf)
%ifnarch %{arm}
BuildRequires:  pkgconfig(heynoti)
%endif
BuildRequires:  pkgconfig(dlog)
BuildRequires:  pkgconfig(usbutils)
BuildRequires:  pkgconfig(device-node)
BuildRequires:  pkgconfig(edbus)
BuildRequires:  pkgconfig(libxml-2.0)
BuildRequires:  pkgconfig(capi-base-common)
BuildRequires:  systemd-devel
BuildRequires:	pkgconfig(systemd)
BuildRequires:	pkgconfig(sqlite3)
Requires(preun): /usr/bin/systemctl
Requires(post): sys-assert
Requires(post): /usr/bin/systemctl
Requires(post): /usr/bin/vconftool
Requires(postun): /usr/bin/systemctl

%description
deviced

%package deviced
Summary:    deviced daemon
Group:      main
Requires:   %{name} = %{version}-%{release}

%description deviced
deviced daemon.

%package -n libdeviced
Summary:    Deviced library
Group:      Development/Libraries

%description -n libdeviced
Deviced library for device control

%package -n libdeviced-devel
Summary:    Deviced library for (devel)
Group:      Development/Libraries
Requires:   libdeviced = %{version}-%{release}

%description -n libdeviced-devel
Deviced library for device control (devel)

%package -n logd
Summary: logd utils
Group: Framework/system

%description -n logd
Utils for for logd

%package -n liblogd
Summary:	Activity logging API(Development)
Group:		Development/Libraries

%description -n liblogd
logd library.

%package -n liblogd-devel
Summary:	Activity logging (Development)
Summary:    SLP power manager client (devel)
Group:		Development/Libraries

%description -n liblogd-devel
logd API library.

%package -n liblogd-db
Summary:	API to get activity data (Development)
Group:		Development/Libraries

%description -n liblogd-db
logd-db library.

%package -n liblogd-db-devel
Summary:	API to get activity data (Development)
Group:		Development/Libraries

%description -n liblogd-db-devel
logd-db API library.

%prep
%setup -q
export CFLAGS+=" -DMICRO_DD"

%if 0%{?sec_build_binary_debug_enable}
export CFLAGS+=" -DTIZEN_DEBUG_ENABLE"
%endif

%if 0%{?tizen_build_binary_release_type_eng}
export CFLAGS+=" -DTIZEN_ENGINEER_MODE"
%define ENGINEER_MODE 1
%else
%define ENGINEER_MODE 0
%endif

%ifarch %{arm}
%define ARCH arm
%else
%define ARCH emulator
%endif
cmake . -DCMAKE_INSTALL_PREFIX=%{_prefix} -DARCH=%{ARCH}

%build
cp %{SOURCE1001} .
cp %{SOURCE1002} .
cp %{SOURCE1003} .
cp %{SOURCE1004} .

make

%install
rm -rf %{buildroot}
%make_install

mkdir -p %{buildroot}%{_libdir}/systemd/system/multi-user.target.wants
mkdir -p %{buildroot}%{_libdir}/systemd/system/graphical.target.wants
mkdir -p %{buildroot}%{_libdir}/systemd/system/shutdown.target.wants
install -m 0644 %{SOURCE1} %{buildroot}%{_libdir}/systemd/system/deviced.service
install -m 0644 %{SOURCE2} %{buildroot}%{_libdir}/systemd/system/zbooting-done.service
install -m 0644 %{SOURCE3} %{buildroot}%{_libdir}/systemd/system/shutdown-notify.service
install -m 0644 %{SOURCE4} %{buildroot}%{_libdir}/systemd/system/deviced-pre.service
install -m 0644 %{SOURCE5} %{buildroot}%{_libdir}/systemd/system/devicectl-start@.service
install -m 0644 %{SOURCE6} %{buildroot}%{_libdir}/systemd/system/devicectl-stop@.service
ln -s ../deviced.service %{buildroot}%{_libdir}/systemd/system/multi-user.target.wants/deviced.service
# Temporary symlink
ln -s deviced.service %{buildroot}%{_libdir}/systemd/system/system-server.service
ln -s ../zbooting-done.service %{buildroot}%{_libdir}/systemd/system/graphical.target.wants/zbooting-done.service
ln -s ../shutdown-notify.service %{buildroot}%{_libdir}/systemd/system/shutdown.target.wants/shutdown-notify.service
ln -s ../devicectl-stop@.service %{buildroot}%{_libdir}/systemd/system/shutdown.target.wants/devicectl-stop@display.service
mkdir -p %{buildroot}%{_sysconfdir}/systemd/default-extra-dependencies/ignore-units.d/
ln -s %{_libdir}/systemd/system/deviced.service %{buildroot}%{_sysconfdir}/systemd/default-extra-dependencies/ignore-units.d/

mkdir -p %{buildroot}%{_datadir}/license
cp LICENSE.Apache-2.0 %{buildroot}/usr/share/license/%{name}
cp LICENSE.Apache-2.0 %{buildroot}/usr/share/license/libdeviced

%post
#memory type vconf key init
vconftool set -t int memory/sysman/usbhost_status -1 -i -s system::vconf_system
vconftool set -t int memory/sysman/charger_status 0 -i -s system::vconf_system
vconftool set -t int memory/sysman/charge_now 0 -i -s system::vconf_system
vconftool set -t int memory/sysman/battery_status_low -1 -i -s system::vconf_system
vconftool set -t int memory/sysman/battery_capacity -1 -i -s system::vconf_system
vconftool set -t int memory/sysman/usb_status -1 -i -s system::vconf_system
vconftool set -t int memory/sysman/factory_mode 0 -i -s system::vconf_system
vconftool set -t int memory/sysman/stime_changed 0 -i -s system::vconf_system
vconftool set -t int memory/sysman/power_off 0 -u 5000 -i -f -s system::vconf_system
vconftool set -t int memory/deviced/boot_power_on 0 -u 5000 -i -f -s system::vconf_system
vconftool set -t int memory/sysman/battery_level_status -1 -i -s system::vconf_system

#db type vconf key init
vconftool set -t bool db/private/deviced/lcd_brightness_init 0 -i -s system::vconf_system

vconftool set -t int memory/pm/state 0 -i -g 5000 -s system::vconf_system
vconftool set -t int memory/pm/camera_status 0 -i -s system::vconf_system
vconftool set -t int memory/pm/battery_timetofull -1 -i -s system::vconf_system
vconftool set -t int memory/pm/battery_timetoempty -1 -i -s system::vconf_system
vconftool set -t int memory/pm/sip_status 0 -i -g 5000 -s system::vconf_system
vconftool set -t int memory/pm/custom_brightness_status 0 -i -g 5000 -s system::vconf_system
vconftool set -t bool memory/pm/brt_changed_lpm 0 -i -s system::vconf_system
vconftool set -t int memory/pm/current_brt 60 -i -g 5000 -s system::vconf_system
vconftool set -t int memory/pm/lcdoff_source 0 -i -g 5000 -s system::vconf_system
vconftool set -t int memory/pm/key_ignore 0 -i -g 5000 -s system::vconf_system

#USB client
vconftool set -t int memory/usb/cur_mode "0" -u 0 -i -s system::vconf_system
vconftool set -t int db/usb/sel_mode "1" -s system::vconf_system
vconftool set -t int db/private/usb/usb_control "1" -u 0 -i -s system::vconf_system
vconftool set -t int memory/private/usb/conf_enabled "0" -u 0 -i -s system::vconf_system

systemctl daemon-reload
if [ $1 == 1 ]; then
    systemctl restart deviced.service
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
%{_bindir}/deviced-pre.sh
%{_bindir}/deviced
%{_bindir}/devicectl
%{_bindir}/deviced-auto-test
%{_libdir}/systemd/system/deviced.service
%{_libdir}/systemd/system/multi-user.target.wants/deviced.service
%{_sysconfdir}/systemd/default-extra-dependencies/ignore-units.d/deviced.service
# Temporary symlink service
%{_libdir}/systemd/system/system-server.service
%{_libdir}/systemd/system/zbooting-done.service
%{_libdir}/systemd/system/graphical.target.wants/zbooting-done.service
%{_libdir}/systemd/system/shutdown-notify.service
%{_libdir}/systemd/system/shutdown.target.wants/shutdown-notify.service
%{_libdir}/systemd/system/deviced-pre.service
%{_libdir}/systemd/system/devicectl-stop@.service
%{_libdir}/systemd/system/devicectl-start@.service
%{_libdir}/systemd/system/shutdown.target.wants/devicectl-stop@display.service
%{_datadir}/license/%{name}
%{_datadir}/deviced/usb-configurations/*
%{_sysconfdir}/smack/accesses2.d/deviced.rule

%manifest deviced.manifest
%attr(110,root,root) /opt/etc/dump.d/module.d/dump_pm.sh
%{_sysconfdir}/deviced/display.conf
%{_sysconfdir}/deviced/mmc.conf
%{_sysconfdir}/deviced/battery.conf
%{_sysconfdir}/deviced/pmqos.conf

%attr(750,root,root)%{_bindir}/start_dr.sh
%if %ENGINEER_MODE
%attr(750,root,root) %{_bindir}/set_usb_debug.sh
%attr(750,root,root) %{_bindir}/direct_set_debug.sh
%endif

%files -n libdeviced
%defattr(-,root,root,-)
%{_libdir}/libdeviced.so.*
%{_datadir}/license/libdeviced
%manifest libdeviced.manifest

%files -n libdeviced-devel
%defattr(-,root,root,-)
%{_includedir}/deviced/*.h
%{_libdir}/libdeviced.so
%{_libdir}/pkgconfig/deviced.pc

%files -n logd

%files -n liblogd
%manifest liblogd.manifest

%files -n liblogd-devel

%files -n liblogd-db
%manifest liblogd-db.manifest

%files -n liblogd-db-devel
