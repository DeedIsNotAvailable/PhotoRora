Name:       ru.omgtu.PhotoRora
Summary:    Photo editor with AI offline
Version:    0.1
Release:    1
License:    BSD-3-Clause
URL:        https://auroraos.ru
Source0:    %{name}-%{version}.tar.bz2

%global __provides_exclude_from ^%{_datadir}/%{name}/lib/.*$
%global __requires_exclude ^libonnxruntime.*$

Requires:   sailfishsilica-qt5 >= 0.10.9
BuildRequires:  pkgconfig(auroraapp)
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5Qml)
BuildRequires:  pkgconfig(Qt5Quick)

%description
PhotoRora — offline photo editor with AI background removal and filters.

%{?aurora_application_metadata}

%prep
%autosetup

%build
%qmake5
%make_build

%install
%make_install
%{?aurora_application_install}

%files
%defattr(-,root,root,-)
%{_bindir}/%{name}
%defattr(644,root,root,-)
%{_datadir}/%{name}
%{_datadir}/applications/%{name}.desktop
%{_datadir}/icons/hicolor/*/apps/%{name}.png
%{?aurora_application_files}


