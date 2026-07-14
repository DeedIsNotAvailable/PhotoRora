%define _cmake_skip_rpath %{nil}

%global __provides_exclude_from ^%{_datadir}/%{name}/lib/.*$
%global __requires_exclude ^(libonnxruntime.*|libonnx.*|libprotobuf.*|libabsl.*|libre2.*|libnsync.*|libcpuinfo.*|libflatbuffers.*|libdate.*|libXNNPACK.*|libpthreadpool.*|libatomic.*)$

Name:       ru.omgtu.PhotoRora
Summary:    Photo editor with AI offline
Version:    0.1
Release:    1
License:    BSD-3-Clause
URL:        https://auroraos.ru
Source0:    %{name}-%{version}.tar.bz2

Requires:   sailfishsilica-qt5 >= 0.10.9
BuildRequires:  pkgconfig(auroraapp)
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5Gui)
BuildRequires:  pkgconfig(Qt5Qml)
BuildRequires:  pkgconfig(Qt5Quick)
BuildRequires:  conan

%description
PhotoRora - offline photo editor with AI background removal and filters.

%prep
%autosetup

%build
CONAN_LIB_DIR="%{_builddir}/conan-libs/"
%{set_build_flags}
rm -f "$CONAN_LIB_DIR/conanrun.sh"
conan-with-aurora-profile export "%{_sourcedir}/../conan-recipes/flatbuffers" --version=1.12.0 --user=aurora
conan-with-aurora-profile export "%{_sourcedir}/../conan-recipes/onnxruntime" --version=1.17.3 --user=aurora
conan-install-if-modified --source-folder="%{_sourcedir}/.." --output-folder="$CONAN_LIB_DIR" -vwarning --build=missing -c tools.cmake:cmake_program=/usr/bin/cmake -o onnxruntime/*:shared=True -o onnxruntime/*:with_cuda=False -o onnxruntime/*:with_xnnpack=False -o flatbuffers/*:shared=False -o flatbuffers/*:header_only=True
PKG_CONFIG_PATH="$CONAN_LIB_DIR:$PKG_CONFIG_PATH"
export PKG_CONFIG_PATH

%cmake -GNinja -DCMAKE_SYSTEM_PROCESSOR=%{_arch}
%ninja_build

%install
%ninja_install

EXECUTABLE="%{buildroot}/%{_bindir}/%{name}"
CONAN_LIB_DIR="%{_builddir}/conan-libs/"
SHARED_LIBRARIES="%{buildroot}/%{_datadir}/%{name}/lib"
mkdir -p "$SHARED_LIBRARIES"
if [ "%{_arch}" = "x86_64" ]; then
    LDD_WRAPPER_DIR="%{_builddir}/.ldd-wrapper"
    mkdir -p "$LDD_WRAPPER_DIR"
    cat > "$LDD_WRAPPER_DIR/ldd" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

rc=0
many=0
if [ "$#" -gt 1 ]; then
    many=1
fi

for f in "$@"; do
    if [ "$many" -eq 1 ]; then
        echo "${f}:"
    fi
    LD_PRELOAD= /lib64/ld-linux-x86-64.so.2 --library-path "${LD_LIBRARY_PATH:-}" --list "$f" || rc=$?
done

exit "$rc"
EOF
    chmod +x "$LDD_WRAPPER_DIR/ldd"
    export PATH="$LDD_WRAPPER_DIR:$PATH"
fi
conan-deploy-libraries "$EXECUTABLE" "$CONAN_LIB_DIR" "$SHARED_LIBRARIES"

%files
%defattr(-,root,root,-)
%{_bindir}/%{name}
%defattr(644,root,root,-)
%{_datadir}/%{name}
%{_datadir}/applications/%{name}.desktop
%{_datadir}/icons/hicolor/*/apps/%{name}.png
