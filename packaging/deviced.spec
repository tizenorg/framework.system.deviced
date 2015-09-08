Name:       deviced
Summary:    deviced
Version:    1.0.0
Release:    1
Group:      Framework/system
License:    Apache-2.0 and BSD-2-Clause and BSD-1.0
Source0:    %{name}-%{version}.tar.gz
Source1:    %{name}.service
Source2:    zbooting-done.service
Source3:    shutdown-notify.service
Source4:    deviced-pre.service
Source5:    devicectl-start@.service
Source6:    devicectl-stop@.service
Source7:    sdb-prestart.service
Source1001: deviced.manifest
Source1002: libdeviced.manifest

BuildRequires:  cmake
BuildRequires:  libattr-devel
BuildRequires:  gettext-devel
BuildRequires:  pkgconfig(ecore)
BuildRequires:  pkgconfig(vconf)
BuildRequires:  pkgconfig(dlog)
BuildRequires:  pkgconfig(device-node)
BuildRequires:  pkgconfig(edbus)
BuildRequires:  pkgconfig(capi-base-common)
BuildRequires:  systemd-devel
BuildRequires:	pkgconfig(systemd)
BuildRequires:	pkgconfig(sqlite3)
BuildRequires:	pkgconfig(libbuxton)
BuildRequires:	pkgconfig(storage)
BuildRequires:	pkgconfig(dbus-glib-1)
BuildRequires:	pkgconfig(journal)
BuildRequires:	pkgconfig(libarchive)
BuildRequires:	pkgconfig(sensor)
BuildRequires:	pkgconfig(tapi)

Requires(preun): /usr/bin/systemctl
Requires(post): sys-assert
Requires(post): /usr/bin/systemctl
Requires(post): /usr/bin/vconftool
Requires(post): pulseaudio
Requires(post): buxton
Requires(post): pkgconfig(journal)
Requires(post): pkgconfig(libarchive)
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

%prep
%setup -q
%if "%{?tizen_profile_name}" == "wearable"
export CFLAGS+=" -DMICRO_DD"
%endif

export CFLAGS+=" -DTIZEN_DEBUG_ENABLE"
export CFLAGS+=" -DTIZEN_ENGINEER_MODE"
%define ENGINEER_MODE 1
%define SDB_PRESTART 1

%ifarch %{arm}
%define ARCH arm
%else
%define ARCH emulator
%endif
cmake . -DCMAKE_INSTALL_PREFIX=%{_prefix} -DARCH=%{ARCH}

%build
cp %{SOURCE1001} .
cp %{SOURCE1002} .

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

%if %SDB_PRESTART
mkdir -p %{buildroot}%{_libdir}/systemd/system/basic.target.wants
install -m 0644 %{SOURCE7} %{buildroot}%{_libdir}/systemd/system/sdb-prestart.service
ln -s ../sdb-prestart.service %{buildroot}%{_libdir}/systemd/system/basic.target.wants/sdb-prestart.service
%endif

mkdir -p %{buildroot}%{_datadir}/license
cp LICENSE %{buildroot}/usr/share/license/%{name}
cp LICENSE %{buildroot}/usr/share/license/libdeviced

%post
%if "%{?tizen_profile_name}" == "wearable"
%else
#memory type vconf key init
vconftool set -t int memory/sysman/cradle_status 0 -i -s system::vconf_system
vconftool set -t int memory/sysman/sliding_keyboard -1 -i -s system::vconf_system
vconftool set -t int memory/sysman/hdmi 0 -i -s system::vconf_system
%endif

#memory type vconf key init
vconftool set -t int memory/sysman/mmc 0 -i -s system::vconf_system
vconftool set -t int memory/sysman/earjack_key 0 -i -s system::vconf_system
vconftool set -t int memory/sysman/added_usb_storage 0 -i -s system::vconf_system
vconftool set -t int memory/sysman/removed_usb_storage 0 -i -s system::vconf_system
vconftool set -t int memory/sysman/earjack -1 -i -s system::vconf_system
vconftool set -t int memory/sysman/mmc_mount -1 -i -s system::vconf_system
vconftool set -t int memory/sysman/mmc_unmount -1 -i -s system::vconf_system
vconftool set -t int memory/sysman/mmc_format -1 -i -s system::vconf_system
vconftool set -t int memory/sysman/mmc_format_progress 0 -i -s system::vconf_system
vconftool set -t int memory/sysman/mmc_err_status 0 -i -s system::vconf_system
vconftool set -t int memory/private/sysman/enhance_pid 0 -i -s system::vconf_system
vconftool set -t int memory/private/sysman/siop_disable 0 -i -s system::vconf_system
vconftool set -t string memory/private/sysman/added_storage_uevent "" -i -s system::vconf_system
vconftool set -t string memory/private/sysman/removed_storage_uevent "" -u 5000 -i -s system::vconf_system
vconftool set -t string memory/private/sysman/siop_level_uevent "" -i -s system::vconf_system
#db type vconf key init
vconftool set -t int db/sysman/mmc_dev_changed 0 -i -s system::vconf_system
vconftool set -t int db/private/sysman/enhance_mode 0 -i -s system::vconf_system
vconftool set -t int db/private/sysman/enhance_scenario 0 -i -s system::vconf_system
vconftool set -t int db/private/sysman/enhance_tone 0 -i -s system::vconf_system
vconftool set -t int db/private/sysman/enhance_outdoor 0 -i -s system::vconf_system
vconftool set -t string db/private/sysman/mmc_device_id "" -i -s system::vconf_system

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
vconftool set -t int memory/sysman/low_memory 1 -i -s system::vconf_system

#db type vconf key init
vconftool set -t int db/private/sysman/cool_down_mode 0 -i -s system::vconf_system
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

if [ $1 == 2 ]; then # upgrade begins, it's rpm -Uvh
    #buxton could be started by socket activation
   systemctl start buxton.service
fi

%pre
#without updating services "daemon-reload" following systemctl stop is wait a
# lot of time
systemctl daemon-reload
if [ $1 == 2 ]; then # it's upgrade, for rpm -Uvh
    systemctl stop buxton.service
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
%{_datadir}/dbus-1/services/org.tizen.system.deviced-auto-test.service
%{_libdir}/systemd/system/deviced-auto-test.service
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
%if %SDB_PRESTART
%{_libdir}/systemd/system/sdb-prestart.service
%{_libdir}/systemd/system/basic.target.wants/sdb-prestart.service
%endif
%{_datadir}/license/%{name}
%{_sysconfdir}/smack/accesses.d/deviced.efl
/etc/deviced/pass.conf
/etc/deviced/usb-client-configuration.conf
/etc/deviced/usb-client-operation.conf
%if "%{?tizen_profile_name}" == "wearable"
%else
/etc/deviced/led.conf
%{_bindir}/movi_format.sh
%{_bindir}/mmc-smack-label
%{_bindir}/fsck_msdosfs
%{_bindir}/newfs_msdos
%{_datadir}/license/fsck_msdosfs
%{_datadir}/license/newfs_msdos
%endif

%manifest deviced.manifest
%attr(110,root,root) /opt/etc/dump.d/module.d/dump_pm.sh
%{_sysconfdir}/deviced/display.conf
%{_sysconfdir}/deviced/mmc.conf
%{_sysconfdir}/deviced/battery.conf
%{_sysconfdir}/deviced/pmqos.conf
%{_sysconfdir}/deviced/storage.conf
%{_sysconfdir}/deviced/haptic.conf

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
