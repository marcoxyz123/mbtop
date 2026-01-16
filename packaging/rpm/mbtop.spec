Name:           mbtop
Version:        1.0.0
Release:        1%{?dist}
Summary:        Terminal-based system monitor

License:        Apache-2.0
URL:            https://github.com/marcoxyz123/mbtop
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  gcc-c++ >= 10
BuildRequires:  make
BuildRequires:  lowdown

%description
mbtop is a powerful terminal-based resource monitor that shows usage
and stats for processor, memory, disks, network and processes.

Fork of btop++ with enhanced Apple Silicon support on macOS.
On Linux, provides standard system monitoring capabilities.

Features:
- Real-time CPU, memory, disk, and network monitoring
- Interactive process management
- Multiple color themes
- Mouse support
- Vim-style keyboard navigation

%prep
%autosetup

%build
%make_build

%install
%make_install PREFIX=%{_prefix}

%files
%license LICENSE
%doc README.md
%{_bindir}/mbtop
%{_datadir}/mbtop/
%{_datadir}/applications/mbtop.desktop
%{_datadir}/icons/hicolor/48x48/apps/mbtop.png
%{_datadir}/icons/hicolor/scalable/apps/mbtop.svg
%{_mandir}/man1/mbtop.1*

%changelog
* Thu Jan 16 2025 Marco Berger <marco@berger.sx> - 1.0.0-1
- Initial release of mbtop
- Fork of btop++ with Apple Silicon enhancements
- GPU, Power, ANE monitoring on macOS
- Full system monitoring on Linux
