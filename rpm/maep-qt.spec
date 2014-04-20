# RPM spec file for Maep.
# This file is used to build Redhat Package Manager packages for
# Maep.  Such packages make it easy to install and uninstall
# the library and related files from binaries or source.
#
# RPM. To build, use the command: rpmbuild --clean -ba maep-qt.spec
#

Name: harbour-maep-qt

# Harbour requirements.
#%define __provides_exclude_from ^%{_datadir}/.*$
#%define __requires_exclude ^libjpeg.*|libcairo.*|libsoup-2.4.*|libgconf-2.*|libxml2.*|libQt5Positioning.*|libdl.*|libdbus-glib-1.*|libpixman-1.*|libfontconfig.*|libfreetype.*|libxcb-shm.*|libxcb-render.*|libxcb.*|libXrender.*|libX11.*|libXau.*|libexpat.*$

Summary: Map browser with GPS capabilities
Version: 1.4.0
Release: 1
Group: Applications/Engineering
License: GPLv2
Source: %{name}-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-buildroot
Requires: sailfishsilica-qt5
Requires: mapplauncherd-booster-silica-qt5
Requires: qt5-qtdeclarative-import-positioning
Requires: qt5-qtdeclarative-import-folderlistmodel
BuildRequires: pkgconfig(qdeclarative5-boostable)
BuildRequires: pkgconfig(Qt5Core)
BuildRequires: pkgconfig(Qt5Qml)
BuildRequires: pkgconfig(Qt5Quick)
BuildRequires: pkgconfig(Qt5Positioning)
BuildRequires: pkgconfig(gobject-2.0)
BuildRequires: pkgconfig(cairo)
BuildRequires: pkgconfig(libsoup-2.4)
BuildRequires: pkgconfig(gconf-2.0)
BuildRequires: pkgconfig(libxml-2.0)
BuildRequires: pkgconfig(libcurl)
BuildRequires: libjpeg-turbo-devel

%description
Maep is a tile based map utility for services like OpenStreetMap, Google maps
and Virtual earth. This is the same map renderer that is also being used by
GPXView and OSM2Go.


%prep
rm -rf $RPM_BUILD_ROOT
%setup -q -n %{name}-%{version}

%build
rm -rf tmp
mkdir tmp
cd tmp
%qmake5 -o Makefile ../src/maep.pro
make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
cd tmp
%qmake5_install
# Copy here the blacklisted libraries
#install -d %{buildroot}/usr/share/%{name}/lib
#install -m 644 -p /usr/lib/libjpeg.so.62 %{buildroot}/usr/share/%{name}/lib/
#install -m 644 -p /usr/lib/libcairo.so.2 %{buildroot}/usr/share/%{name}/lib/
#install -m 644 -p /usr/lib/libsoup-2.4.so.1 %{buildroot}/usr/share/%{name}/lib/
#install -m 644 -p /usr/lib/libgconf-2.so.4 %{buildroot}/usr/share/%{name}/lib/
#install -m 644 -p /usr/lib/libxml2.so.2 %{buildroot}/usr/share/%{name}/lib/
#install -m 644 -p /usr/lib/libQt5Positioning.so.5 %{buildroot}/usr/share/%{name}/lib/
#install -m 644 -p /lib/libdl.so.2 %{buildroot}/usr/share/%{name}/lib/
#install -m 644 -p /usr/lib/libdbus-glib-1.so.2 %{buildroot}/usr/share/%{name}/lib/
#install -m 644 -p /usr/lib/libpixman-1.so.0 %{buildroot}/usr/share/%{name}/lib/
#install -m 644 -p /usr/lib/libfontconfig.so.1 %{buildroot}/usr/share/%{name}/lib/
#install -m 644 -p /usr/lib/libfreetype.so.6 %{buildroot}/usr/share/%{name}/lib/
#install -m 644 -p /usr/lib/libxcb-shm.so.0 %{buildroot}/usr/share/%{name}/lib/
#install -m 644 -p /usr/lib/libxcb-render.so.0 %{buildroot}/usr/share/%{name}/lib/
#install -m 644 -p /usr/lib/libxcb.so.1 %{buildroot}/usr/share/%{name}/lib/
#install -m 644 -p /usr/lib/libXrender.so.1 %{buildroot}/usr/share/%{name}/lib/
#install -m 644 -p /usr/lib/libX11.so.6 %{buildroot}/usr/share/%{name}/lib/
#install -m 644 -p /usr/lib/libXau.so.6 %{buildroot}/usr/share/%{name}/lib/
#install -m 644 -p /usr/lib/libexpat.so.1 %{buildroot}/usr/share/%{name}/lib/

%files
%defattr(-,root,root,-)
/usr/share/applications
/usr/share/icons
/usr/share/%{name}
/usr/bin

%changelog
* Sat Apr 19 2014 - Damien Caliste <dcaliste@free.fr> 1.4.0-1
- rework the display layout, using swipe to switch from search to
  track actions.
- add support for waypoints in tracks. waypoints are editable
  through the GUI (addition and modification). Close: #5
- correct track segment handling for tracks, giving correct track duration.
- use horizontal accuracy of GPS fix to enhanced track length and duration
  calculation.
- add track info on cover if available.

* Fri Mar 21 2014 - Damien Caliste <dcaliste@free.fr> 1.3.7-13
- modify the layout of the track management drawer to highlight possibilities when there is no track.
- add creation time of track in the track management drawer.
- disable track capture when loading a track to avoid unwanted expansion of already saved tracks.

* Thu Mar 13 2014 - Damien Caliste <dcaliste@free.fr> 1.3.7-12
- implement track length and duration measurements.
- correct the autosave functionality for tracks.
- add a setting for autosave period.
- add a bottom drawer to display track information.

* Thu Mar 06 2014 - Damien Caliste <dcaliste@free.fr> 1.3.7-11
- correct a memory leak in the OSD rendering code.

* Mon Jan 27 2014 - Damien Caliste <dcaliste@free.fr> 1.3.7-10
- use the new api.geonames.org (in between, ws.geonames.org has reappered again) ;
- use an approximation to display the direction when GPS is used, waiting for a Qt bug being fixed and get this information from GPS directly.

* Wed Jan 22 2014 - Damien Caliste <dcaliste@free.fr> 1.3.7-9
- auto centering is correctly working (i.e. tapping on the GPS icon highlight it and the map is auto-centered, and moving the map by hand cancel it).
- accept in the export dialog of a track by swyping or tapping on the header works as expected.
- use a correct user-agent to download tiles (maep-libsoup/1.3.7 instead of a browser-like one).
- display the copyright for each map in the source selection dialog, add also a link to the full copyright page for each map.

* Tue Jan 14 2014 - Damien Caliste <dcaliste@free.fr> 1.3.7-8
- correct a bug not activating the GPS, introduced in 1.3.7-7.

* Mon Jan 13 2014 - Damien Caliste <dcaliste@free.fr> 1.3.7-7
- import / export layout are now OK.
- properly save the exported track.
- try to correct a crashing bug after searching for something (an issue in Curl in fact).
- use bigger icons for wikipedia data.
- use a lighter gradient for the bottom toolbar and shift a bit the icons of the toolbar to the bottom.
- use a gconf parameter to change the GPS refresh rate (a parameter page to change this in the UI should appear next).

* Thu Jan 09 2014 - Damien Caliste <dcaliste@free.fr> 1.3.7-6
- the source selector is now working.
- when tracking is enabled, the map do not zoom anymore to the entire earth when one get the first GPS fix.
- the auto-center button is working, but still a bit difficult to activate…
- a button has been added near the search field to reopen the last search results and results appear in a larger item size for big fingers !

* Mon Dec 02 2013 - Damien Caliste <dcaliste@free.fr> 1.3.7-5
- The source selection and the zoom is now done via Sailfish styled controls at the bottom of the map, using a gradient transparency to the background image as background.
- The top of the map area has rounded corner.
- Mapquest is used as rendering for OSM II.
- Port the missing bug and donate pages in the about section.
- Open links in the desktop browser instead of inside Mæp.
- Correct the spurious fog effect on the cover.
- Use previews in the source selection.
- Rename everything to validate harbour rules on naming.

* Mon Nov 25 2013 - Damien Caliste <dcaliste@free.fr> 1.3.7-3
- JPG (google and virtual earth sattelite) tiles are back.
- filechooser for Import / export of tracks.
- about window.
- thanks to Thomas Perl, some corrections on packaging.
- bug of pull down menu below the map corrected.
- bug of map resize on device rotated (maybe) fixed.
