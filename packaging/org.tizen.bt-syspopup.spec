%define _usrdir /usr
%define _appdir %{_usrdir}/apps

Name:       org.tizen.bt-syspopup
Summary:    bluetooth system-popup application (bluetooth system popup)
Version:    0.2.103
Release:    1
%if "%{?tizen_profile_name}" == "wearable"
VCS:        magnolia/apps/home/bt-syspopup#bt-syspopup_0.2.78-63-g4343fdeb355156f438784eceb6261b10b880ce1b
%endif
Group:      main
License:    Flora Software License, Version 1.1
Source0:    %{name}-%{version}.tar.gz
Requires(post): sys-assert
BuildRequires:  pkgconfig(evas)
%if "%{?tizen_profile_name}" == "mobile"
BuildRequires:  pkgconfig(efl-assist)
%endif
BuildRequires:  pkgconfig(ecore-input)
BuildRequires:  pkgconfig(ethumb)
BuildRequires:  pkgconfig(elementary)
%if "%{?tizen_profile_name}" == "wearable"
BuildRequires:  efl-assist-devel
%endif
BuildRequires:  pkgconfig(efreet)
BuildRequires:  pkgconfig(sensor)
BuildRequires:  pkgconfig(capi-appfw-application)
BuildRequires:  pkgconfig(utilX)
BuildRequires:  pkgconfig(syspopup)
%if "%{?tizen_profile_name}" == "wearable"
BuildRequires:  pkgconfig(syspopup-caller)
%endif
BuildRequires:  pkgconfig(dlog)
BuildRequires:  pkgconfig(deviced)
BuildRequires:  pkgconfig(dbus-glib-1)
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(aul)
BuildRequires:  pkgconfig(capi-network-bluetooth)
BuildRequires:  pkgconfig(feedback)
%if "%{?tizen_profile_name}" == "mobile"
BuildRequires:  pkgconfig(notification)
%endif
BuildRequires:  edje-tools

BuildRequires:  cmake
BuildRequires:  gettext-devel
BuildRequires:  hash-signer

%description
bluetooth system-popup application (bluetooth system popup).

%prep
%setup -q

%build
#%if 0%{?sec_build_binary_debug_enable}
%if "%{?tizen_profile_name}" == "wearable"
export CFLAGS="$CFLAGS -DTIZEN_ENGINEER_MODE -DTIZEN_WEARABLE"
export CXXFLAGS="$CXXFLAGS -DTIZEN_ENGINEER_MODE"
export FFLAGS="$FFLAGS -DTIZEN_ENGINEER_MODE"
%elseif "%{?tizen_profile_name}" == "mobile"
echo 1234mobile
export CFLAGS="$CFLAGS -DTIZEN_DEBUG_ENABLE -DTIZEN_MOBILE"
export CXXFLAGS="$CXXFLAGS -DTIZEN_DEBUG_ENABLE"
export FFLAGS="$FFLAGS -DTIZEN_DEBUG_ENABLE"
%endif
#%endif
%if "%{?tizen_profile_name}" == "wearable"
export CFLAGS+=" -fpie -fvisibility=hidden"
export LDFLAGS+=" -Wl,--rpath=/usr/lib -Wl,--as-needed -Wl,--unresolved-symbols=ignore-in-shared-libs -pie"
%elseif "%{?tizen_profile_name}" == "mobile"
export CFLAGS+=" -fpie -fvisibility=hidden -D__ENABLE_BLUEZ5__"
%endif

cmake . -DCMAKE_INSTALL_PREFIX=%{_appdir}/org.tizen.bt-syspopup
make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
%make_install
PKG_ID=org.tizen.bt-syspopup
%define tizen_sign 1
%if "%{?tizen_profile_name}" == "wearable"
%define tizen_sign_base /usr/apps/${PKG_ID}
%elseif "%{?tizen_profile_name}" == "mobile"
%define tizen_sign_base /usr/apps/org.tizen.bt-syspopup
%endif
%define tizen_sign_level platform
%define tizen_author_sign 1
%define tizen_dist_sign 1

install -D -m 0644 LICENSE %{buildroot}%{_datadir}/license/org.tizen.bt-syspopup

%if "%{?tizen_profile_name}" == "wearable"
install -D -m 0644 data/org.tizen.bt-syspopup-w.efl %{buildroot}/etc/smack/accesses.d/org.tizen.bt-syspopup.efl
install -D -m 0644 data/org.tizen.bt-syspopup-w.xml %{buildroot}%{_usrdir}/share/packages/org.tizen.bt-syspopup.xml
%elseif "%{?tizen_profile_name}" == "mobile"
install -D -m 0644 data/org.tizen.bt-syspopup-m.efl %{buildroot}/etc/smack/accesses.d/org.tizen.bt-syspopup.efl
install -D -m 0644 data/org.tizen.bt-syspopup-m.xml %{buildroot}%{_usrdir}/share/packages/org.tizen.bt-syspopup.xml
%endif

%post

%files
%if "%{?tizen_profile_name}" == "wearable"
%manifest org.tizen.bt-syspopup-w.manifest
%elseif "%{?tizen_profile_name}" == "mobile"
%manifest org.tizen.bt-syspopup-m.manifest
%endif
%defattr(-,root,root,-)
/etc/smack/accesses.d/org.tizen.bt-syspopup.efl
%{_usrdir}/share/packages/org.tizen.bt-syspopup.xml
%{_appdir}/org.tizen.bt-syspopup/bin/bt-syspopup
%{_appdir}/org.tizen.bt-syspopup/res/edje/*.edj
%{_appdir}/org.tizen.bt-syspopup/author-signature.xml
%{_appdir}/org.tizen.bt-syspopup/signature1.xml
%{_usrdir}/share/icons/default/small/org.tizen.bt-syspopup.png
%{_datadir}/license/org.tizen.bt-syspopup
%if "%{?tizen_profile_name}" == "wearable"
/usr/apps/org.tizen.bt-syspopup/shared/res/tables/org.tizen.bt-syspopup_ChangeableColorTable.xml
/usr/apps/org.tizen.bt-syspopup/shared/res/tables/org.tizen.bt-syspopup_FontInfoTable.xml
%endif
