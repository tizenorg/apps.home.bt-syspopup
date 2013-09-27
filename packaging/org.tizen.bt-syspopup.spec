%define _optdir /opt
%define _usrdir /usr
%define _appdir %{_optdir}/apps

Name:       org.tizen.bt-syspopup
Summary:    bluetooth system-popup application (bluetooth system popup)
Version: 0.2.56
Release:    1
Group:      main
License:    Flora Software License
Source0:    %{name}-%{version}.tar.gz
Requires(post): sys-assert
BuildRequires:  pkgconfig(evas)
BuildRequires:  pkgconfig(ecore-input)
BuildRequires:  pkgconfig(ethumb)
BuildRequires:  pkgconfig(elementary)
BuildRequires:  efl-assist-devel
BuildRequires:  pkgconfig(efreet)
BuildRequires:  pkgconfig(sysman)
BuildRequires:  pkgconfig(sensor)
BuildRequires:  pkgconfig(appcore-efl)
BuildRequires:  pkgconfig(devman)
BuildRequires:  pkgconfig(utilX)
BuildRequires:  pkgconfig(syspopup)
BuildRequires:  pkgconfig(dlog)
BuildRequires:  pkgconfig(pmapi)
BuildRequires:  pkgconfig(dbus-glib-1)
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(devman_haptic)
BuildRequires:  pkgconfig(aul)
BuildRequires:  pkgconfig(bluetooth-api)
BuildRequires:  pkgconfig(feedback)
BuildRequires:  sysman-internal-devel
BuildRequires:  edje-tools

BuildRequires:  cmake
BuildRequires:  gettext-devel

%description
bluetooth system-popup application (bluetooth system popup).

%prep
%setup -q


%build
export CFLAGS+=" -fpie -fvisibility=hidden"
export LDFLAGS+=" -Wl,--rpath=/usr/lib -Wl,--as-needed -Wl,--unresolved-symbols=ignore-in-shared-libs -pie"

cmake . -DCMAKE_INSTALL_PREFIX=%{_appdir}/org.tizen.bt-syspopup
make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
%make_install
mkdir -p %{buildroot}/usr/share/license
cp LICENSE.Flora %{buildroot}/usr/share/license/%{name}

%files
%manifest org.tizen.bt-syspopup.manifest
/opt/etc/smack/accesses.d/org.tizen.bt-syspopup.rule
%defattr(-,root,root,-)
%{_usrdir}/share/packages/org.tizen.bt-syspopup.xml
%{_appdir}/org.tizen.bt-syspopup/bin/bt-syspopup
%{_appdir}/org.tizen.bt-syspopup/res/edje/*.edj
%{_optdir}/share/icons/default/small/org.tizen.bt-syspopup.png
/usr/share/license/%{name}
