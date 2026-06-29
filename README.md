# openSUSE Update Applet

A Qt6 system tray applet that periodically checks for openSUSE system updates and notifies you when updates are available.

## Features

- **System tray icon** — green (up-to-date), amber (checking), red (updates available)
- **Automatic checking** — runs `zypper list-updates`, `flatpak remote-ls --updates`, and `snap refresh --list` at a configurable interval
- **Startup check** — automatically checks for updates when the app starts
- **Desktop notifications** — shows update counts per package manager (zypper / flatpak / snap)
- **Update list** — view all available updates in a checkable tree, select which to install
- **Install updates** — install all or selected updates with one click
- **Auto-update** — silently apply updates without interaction (optional)
- **Lock packages** — prevent specific packages from being updated (uses `zypper addlock`)
- **Sudo password management** — store password in system keychain (qtkeychain) to avoid prompts
- **Configurable interval** — 15 min to 24 hours, or manual only
- **Auto-start** — option to start on boot
- **Symbolic icons** — configurable tray icons: colored or system-palette symbolic
- **Light / Dark mode** — automatic theme detection with QSS styling

## Installation

### Option 1: Prebuilt RPM (easiest)

Download the latest `.rpm` from the [Releases](https://github.com/antoan-m/opensuse_autoupdate_applet/releases) page and install:

```bash
sudo zypper install opensuse-update-applet-*.rpm
```

### Option 2: Build from Source

```bash
# Install dependencies
sudo zypper install -y qt6-widgets-devel qt6-svg-devel qt6-network-devel qt6-dbus-devel qtkeychain-qt6-devel cmake gcc-c++

# Build & install
cmake -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
sudo cmake --install build-release
```

### Option 3: Build RPM yourself

```bash
sudo zypper install -y rpm-build
rpmbuild -ba opensuse-update-applet.spec
sudo zypper install ~/rpmbuild/RPMS/x86_64/opensuse-update-applet-*.rpm
```

> See [guide.md](guide.md) for all build and install variants.

## Usage

Run `opensuse-update-applet` from the terminal or your application menu. The app lives in the system tray.

- Right-click the tray icon → **Check Now** to trigger an immediate update check
- Double-click or right-click → **Open Update Manager** to view updates, manage locks, and change settings
- The tray icon turns **red** when updates are available, **amber** while checking, and **green** when up-to-date

## Configuration

Settings are stored in `~/.config/OpenSUSE/UpdateApplet.conf`. The sudo password is stored in your system keychain (KDE Wallet / GNOME Keyring / libsecret).

## License

GNU General Public License v3.0 — see [LICENSE](LICENSE).
