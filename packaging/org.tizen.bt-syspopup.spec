%if %{_repository} == "wearable"
%define _usrdir /usr
%define _appdir %{_usrdir}/apps
%elseif %{_repository} == "mobile"
%define _optdir /opt
%define _usrdir /usr
%define _appdir %{_optdir}/apps
%endif

Name:       org.tizen.bt-syspopup
Summary:    bluetooth system-popup application (bluetooth system popup)
Version: 0.2.56
Release:    1
Group:      main
License:    Flora Software License, Version 1.1
Source0:    %{name}-%{version}.tar.gz
Requires(post): sys-assert
BuildRequires:  pkgconfig(evas)
BuildRequires:  pkgconfig(ecore-input)
BuildRequires:  pkgconfig(ethumb)
BuildRequires:  pkgconfig(elementary)
BuildRequires:  pkgconfig(efreet)
BuildRequires:  pkgconfig(sensor)
BuildRequires:  pkgconfig(utilX)
BuildRequires:  pkgconfig(syspopup)
BuildRequires:  pkgconfig(dlog)
BuildRequires:  pkgconfig(dbus-glib-1)
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(aul)
BuildRequires:  pkgconfig(feedback)
BuildRequires:  edje-tools
BuildRequires:  cmake
BuildRequires:  gettext-devel
%if %{_repository} == "wearable"
BuildRequires:  efl-assist-devel
BuildRequires:  pkgconfig(capi-appfw-application)
BuildRequires:  pkgconfig(deviced)
BuildRequires:  pkgconfig(capi-network-bluetooth)
BuildRequires:  pkgconfig(deviced)
BuildRequires:  hash-signer
%endif
%if %{_repository} == "mobile"
BuildRequires:  pkgconfig(sysman)
BuildRequires:  pkgconfig(appcore-efl)
BuildRequires:  pkgconfig(devman)
BuildRequires:  pkgconfig(pmapi)
BuildRequires:  pkgconfig(devman_haptic)
BuildRequires:  pkgconfig(bluetooth-api)
BuildRequires:  sysman-internal-devel
%endif

%description
bluetooth system-popup application (bluetooth system popup).

%prep
%setup -q


%build
%if %{_repository} == "wearable"
%if 0%{?tizen_build_binary_release_type_eng}
export CFLAGS="$CFLAGS -DTIZEN_ENGINEER_MODE"
export CXXFLAGS="$CXXFLAGS -DTIZEN_ENGINEER_MODE"
export FFLAGS="$FFLAGS -DTIZEN_ENGINEER_MODE"
%endif
%endif
export CFLAGS+=" -fpie -fvisibility=hidden"
export LDFLAGS+=" -Wl,--rpath=/usr/lib -Wl,--as-needed -Wl,--unresolved-symbols=ignore-in-shared-libs -pie"

%if %{_repository} == "wearable"
cd wearable
%elseif %{_repository} == "mobile"
cd mobile
%endif
cmake . -DCMAKE_INSTALL_PREFIX=%{_appdir}/%{name}
make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}

%if %{_repository} == "wearable"
cd wearable
%make_install
PKG_ID=org.tizen.bt-syspopup
%define tizen_sign 1
%define tizen_sign_base /usr/apps/${PKG_ID}
%define tizen_sign_level platform
%define tizen_author_sign 1
%define tizen_dist_sign 1

install -D -m 0644 LICENSE.Flora %{buildroot}%{_datadir}/license/org.tizen.bt-syspopup
%elseif %{_repository} == "mobile"
cd mobile
%make_install
mkdir -p %{buildroot}/usr/share/license
cp LICENSE.Flora %{buildroot}/usr/share/license/%{name}
%endif

%files
%if %{_repository} == "wearable"
%manifest wearable/org.tizen.bt-syspopup.manifest
/etc/smack/accesses2.d/org.tizen.bt-syspopup.rule
%defattr(-,root,root,-)
%{_usrdir}/share/packages/org.tizen.bt-syspopup.xml
%{_appdir}/org.tizen.bt-syspopup/bin/bt-syspopup
%{_appdir}/org.tizen.bt-syspopup/res/edje/*.edj
%{_appdir}/org.tizen.bt-syspopup/res/images/*.png
%{_appdir}/org.tizen.bt-syspopup/author-signature.xml
%{_appdir}/org.tizen.bt-syspopup/signature1.xml
%{_usrdir}/share/icons/default/small/org.tizen.bt-syspopup.png
%{_datadir}/license/org.tizen.bt-syspopup
%elseif %{_repository} == "mobile"
%manifest mobile/org.tizen.bt-syspopup.manifest
/opt/etc/smack/accesses.d/org.tizen.bt-syspopup.rule
%defattr(-,root,root,-)
%{_usrdir}/share/packages/org.tizen.bt-syspopup.xml
%{_appdir}/org.tizen.bt-syspopup/bin/bt-syspopup
%{_appdir}/org.tizen.bt-syspopup/res/edje/*.edj
%{_optdir}/share/icons/default/small/org.tizen.bt-syspopup.png
/usr/share/license/%{name}
%endif
