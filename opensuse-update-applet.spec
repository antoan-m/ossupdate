%define _name opensuse-update-applet

Name:           opensuse-update-applet
Version:        1.2.0
Release:        1
Summary:        System tray applet for openSUSE zypper/flatpak/snap updates
License:        GPL-3.0-only
URL:            https://github.com/antoan-m/opensuse_autoupdate_applet
Source:         %{_name}-%{version}.tar.gz
BuildRequires:  cmake
BuildRequires:  gcc-c++
BuildRequires:  qt6-widgets-devel
BuildRequires:  qt6-svg-devel
BuildRequires:  qt6-network-devel
BuildRequires:  qt6-dbus-devel
BuildRequires:  qtkeychain-qt6-devel
Requires:       libQt6Core6
Requires:       libQt6Gui6
Requires:       libQt6Widgets6
Requires:       libQt6Svg6
Requires:       libQt6Network6
Requires:       libQt6DBus6
Requires:       libqt6keychain1

%description
A Qt6 system tray applet that periodically checks for openSUSE system
updates (zypper dup, flatpak, snap) and notifies you when updates are
available. Supports configurable check intervals, auto-update, package
locking, and sudo password storage via system keychain.

%prep
%setup -q

%build
%cmake -DCMAKE_BUILD_TYPE=Release
%cmake_build

%install
%cmake_install

%files
%license LICENSE
%doc README.md
%{_bindir}/%{_name}
%{_datadir}/applications/%{_name}.desktop
%{_datadir}/icons/hicolor/scalable/apps/%{_name}.svg

%changelog
* Sun Jul 05 2026 antoan-m <antoan@localhost.localdomain> 1.2.0-1
- Fixes:
  - Sort order from Z-A to A-Z
  - Alphabetically sorted update list
  - Columns stretch to fill window width
  - Zypper exit codes 4/5/6/7 treated as success

* Mon Jun 29 2026 antoan-m <antoan@localhost.localdomain> 1.1.0-1
- Self-update via GitHub release checker
- Auto-update app option in settings
- Symbolic tray icons using system palette colors
- RPM packaging with .spec file
