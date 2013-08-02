Name:       syncevolution
Summary:    SyncML Client
Version:    1.3.99.4
Release:    1
Group:      Social & Content/Service
License:    LGPL-2.1+
URL:        http://syncevolution.org
Source0:    %{name}-%{version}.tar.gz
Requires:   ca-certificates
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig
BuildRequires:  pkgconfig(bluez)
BuildRequires:  pkgconfig(dbus-glib-1)
BuildRequires:  pkgconfig(gio-2.0)
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(libpcre)
BuildRequires:  pkgconfig(libsoup-2.4)
BuildRequires:  pkgconfig(icu-i18n)
BuildRequires:  pkgconfig(folks)
BuildRequires:  pkgconfig(folks-eds)
BuildRequires:  boost-devel
BuildRequires:  expat-devel
BuildRequires:  intltool
BuildRequires:  autoconf
BuildRequires:  automake
BuildRequires:  libtool
BuildRequires:  python
BuildRequires:  libxslt-tools
BuildRequires:  libphonenumber-devel


%description
The %{name} package contains the core files required to use
SyncEvolution, a PIM data synchronization solution.



%package synccompare
Summary:    Optional data comparison tool for SyncEvolution
Group:      Social & Content/Service
Requires:   %{name} = %{version}-%{release}

%description synccompare
The %{name}-synccompare package contains a perl script used in
SyncEvolution command line mode for data comparison. It is optional
and not normally called when using a GUI.


%package http-server
Summary:    SyncEvolution HTTP SyncML server
Group:      Social & Content/Service
Requires:   %{name} = %{version}-%{release}
Requires:   python-twisted-web

%description http-server
The %{name}-http-server package contains a HTTP SyncML server
based on the Python Twisted framework.


%package phone-config
Summary:    SyncEvolution phone configuration tool
Group:      Social & Content/Service
Requires:   %{name} = %{version}-%{release}

%description phone-config
The %{name}-http-server package contains a Python script
which determines via trial-and-error the correct parameters
for an unknown phone. Not normally needed, most phones work
with existing templates.


%package devel
Summary:    Development files for %{name}
Group:      Social & Content/Service
Requires:   %{name} = %{version}-%{release}

%description devel
The %{name}-devel package contains libraries and header files for
developing applications that use %{name}.


%package ebook
Summary:    SyncEvolution backend for EDS contacts
Group:      Social & Content/Service
Requires:   %{name} = %{version}-%{release}

%description ebook
The %{name}-ebook package contains a backend which reads/writes
contacts in EDS.


%package ecal
Summary:    SyncEvolution backend for EDS events/tasks/memos
Group:      Social & Content/Service
Requires:   %{name} = %{version}-%{release}

%description ecal
The %{name}-ecal package contains a backend which reads/writes
contacts in EDS.


%package pbap
Summary:    SyncEvolution backend for contact caching via PBAP
Group:      Social & Content/Service
Requires:   %{name} = %{version}-%{release}

%description pbap
The %{name}-pbap package contains a backend which reads/writes
contacts via PBAP.

%prep
%setup -q -n %{name}-%{version}

%build
./autogen.sh

%configure --disable-static \
    'PHONENUMBERS_LIBS=-lphonenumber -lboost_thread' \
    --enable-dbus-service \
    --enable-dbus-service-pim \
    --enable-shared \
    --with-expat=system \
    --enable-release-mode \
    --disable-unit-tests \
    --disable-integration-tests \
    --disable-sqlite

make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
%make_install

rm %{buildroot}/%{_bindir}/synclog2html
rm %{buildroot}/%{_libdir}/syncevolution/backends/platformgnome.so
rm %{buildroot}/%{_libdir}/syncevolution/backends/platformkde.so
%find_lang syncevolution



%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files -f syncevolution.lang
%defattr(-,root,root,-)
%{_bindir}/syncevolution
%{_bindir}/syncevo-webdav-lookup
%{_libdir}/libsynthesis.so.*
%{_libdir}/libsmltk.so.*
%{_libdir}/libsyncevolution.so.*
%{_libdir}/syncevolution/backends/syncactivesync.so
%{_libdir}/syncevolution/backends/syncfile.so
%{_libdir}/syncevolution/backends/syncxmlrpc.so
%{_libdir}/syncevolution/backends/syncaddressbook.so
%{_libdir}/syncevolution/backends/syncsqlite.so
%{_libdir}/syncevolution/backends/syncmaemocal.so
%{_libdir}/syncevolution/backends/syncakonadi.so
%{_libdir}/libgdbussyncevo.*
%{_libdir}/syncevolution/backends/syncqtcontacts.so
%{_libdir}/syncevolution/backends/synckcalextended.so
%{_libdir}/syncevolution/backends/syncdav.so
%{_libexecdir}/syncevo-dbus-server
%{_libexecdir}/syncevo-dbus-server-startup.sh
%{_libexecdir}/syncevo-dbus-helper
%{_libexecdir}/syncevo-local-sync
%{_datadir}/syncevolution
%{_datadir}/doc/syncevolution/*
%{_datadir}/dbus-1/services/org.syncevolution.service
%{_datadir}/dbus-1/services/org._01.pim.contacts.service
%{_sysconfdir}/xdg/autostart/syncevo-dbus-server.desktop


%files synccompare
%defattr(-,root,root,-)
%{_bindir}/synccompare

%files http-server
%defattr(-,root,root,-)
%{_bindir}/syncevo-http-server

%files phone-config
%defattr(-,root,root,-)
%{_bindir}/syncevo-phone-config

%files devel
%defattr(-,root,root,-)
%dir %{_includedir}/synthesis
%{_includedir}/synthesis/*.h
%{_includedir}/syncevo/*.h
%{_libdir}/libsynthesis.so
%{_libdir}/libsmltk.so
%{_libdir}/libsyncevolution.so
%{_libdir}/pkgconfig/syncevolution.pc
%{_libdir}/pkgconfig/synthesis.pc
%{_libdir}/pkgconfig/synthesis-sdk.pc

%files ebook
%defattr(-,root,root,-)
%{_libdir}/syncevolution/backends/syncebook.so

%files ecal
%defattr(-,root,root,-)
%{_libdir}/syncevolution/backends/syncecal.so

%files pbap
%defattr(-,root,root,-)
%{_libdir}/syncevolution/backends/syncpbap.so
