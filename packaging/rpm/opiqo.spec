Name:           opiqo
Version:        0.8.0
Release:        1%{?dist}
Summary:        GTK4 LV2 plugin host with JACK audio backend

License:        MIT AND Apache-2.0 AND BSD-3-Clause
URL:            https://github.com/djshaji/opiqo-linux
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  cmake
BuildRequires:  gcc-c++
BuildRequires:  pkgconfig
BuildRequires:  pkgconfig(gtk4)
BuildRequires:  pkgconfig(jack)
BuildRequires:  pkgconfig(lv2)
BuildRequires:  pkgconfig(lilv-0)
BuildRequires:  pkgconfig(serd-0)
BuildRequires:  pkgconfig(sord-0)
BuildRequires:  pkgconfig(sratom-0)
BuildRequires:  pkgconfig(sndfile)
BuildRequires:  pkgconfig(ogg)
BuildRequires:  pkgconfig(vorbis)
BuildRequires:  pkgconfig(vorbisenc)
BuildRequires:  pkgconfig(opus)
BuildRequires:  pkgconfig(libopusenc)
BuildRequires:  lame-devel
BuildRequires:  flac-devel
BuildRequires:  glib2-devel

%description
Opiqo is a real-time LV2 plugin host for Linux with a GTK4 user interface
and a JACK audio backend. It supports loading and controlling plugins,
recording processed output, and routing audio ports.

%prep
%autosetup

%build
%cmake -DOPIQO_TARGET_PLATFORM=linux
%cmake_build

%install
%cmake_install

%files
%license LICENSE
%{_bindir}/opiqo

%changelog
* Thu Apr 09 2026 GitHub Copilot <copilot@github.com> - 0.8.0-1
- Initial RPM packaging for source RPM generation