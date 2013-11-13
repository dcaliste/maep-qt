# RPM spec file for Maep.
# This file is used to build Redhat Package Manager packages for
# Maep.  Such packages make it easy to install and uninstall
# the library and related files from binaries or source.
#
# RPM. To build, use the command: rpm --clean -ba maep-qt.spec
#

Name: maep-qt
Summary: Map browser with GPS capabilities
Version: 1.3.7
Release: 1
Group: Applications/Engineering
License: GPLv2
Source: %{name}-%{version}.tar.bz2
BuildRoot: %{_tmppath}/%{name}-%{version}-buildroot
Requires: sailfishsilica-qt5
Requires: mapplauncherd-booster-silica-qt5
BuildRequires: pkgconfig(qdeclarative5-boostable)
BuildRequires: pkgconfig(Qt5Core)
BuildRequires: pkgconfig(Qt5Qml)
BuildRequires: pkgconfig(Qt5Quick)

%description
Maep is a tile based map utility for services like OpenStreetMap, Google maps
and Virtual earth. This is the same map renderer that is also being used by
GPXView and OSM2Go.


%prep
rm -rf $RPM_BUILD_ROOT
%setup -q -n %{name}-%{version}

# >> setup
# << setup

%build
# >> build pre
# << build pre

%qtc_qmake5 

%qtc_make %{?jobs:-j%jobs}

# >> build post
# << build post

%install
rm -rf %{buildroot}
# >> install pre
# << install pre
%qmake5_install

# >> install post
# << install post

%files
%defattr(-,root,root,-)
/usr/share/applications
/usr/share/test-qml
/usr/bin
# >> files
# << files
