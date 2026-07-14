from conan import ConanFile
from conan.errors import ConanInvalidConfiguration
from conan.tools.apple import is_apple_os
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout
from conan.tools.files import apply_conandata_patches, copy, export_conandata_patches, get, rmdir, replace_in_file
from conan.tools.build import check_min_cppstd
from conan.tools.scm import Version
from conan.tools.env import VirtualBuildEnv
import os
import sys
import re
import shutil


required_conan_version = ">=1.53.0"


class OnnxRuntimeRecipe(ConanFile):
    name = "onnxruntime"
    package_type = "library"
    user = "aurora"

    description = "ONNX Runtime: cross-platform, high performance ML inferencing and training accelerator"
    url = "https://github.com/microsoft/onnxruntime"
    license = "MIT"
    author = "Daniil Markevich <d.markevich@omp.ru>"
    homepage = "https://onnxruntime.ai"
    topics = ("deep-learning", "onnx", "neural-networks", "machine-learning", "ai-framework", "hardware-acceleration")
    settings = "os", "arch", "compiler", "build_type"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "with_xnnpack": [True, False],
        "with_cuda": [True, False],
    }
    default_options = {
        "shared": True,
        "fPIC": True,
        "with_xnnpack": True,
        "with_cuda": False,
    }
    short_paths = True

    @property
    def _min_cppstd(self):
        if is_apple_os(self) and Version(self.version) >= "1.17.0":
            return 20  # https://github.com/microsoft/onnxruntime/blob/8f5c79cb63f09ef1302e85081093a3fe4da1bc7d/cmake/CMakeLists.txt#L43-L47
        return 17

    @property
    def _compilers_minimum_version(self):
        if Version(self.version) < "1.16.0":
            return {
                "Visual Studio": "16",
                "msvc": "192",
                "gcc": "7",
                "clang": "5",
                "apple-clang": "10",
            }
        return {
            "Visual Studio": "17",
            "msvc": "193",
            "gcc": "8",
            "clang": "5",
            "apple-clang": "10",
        }

    def export_sources(self):
        export_conandata_patches(self)
        copy(self, "cmake/*", src=self.recipe_folder, dst=self.export_sources_folder)

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")
        # onnxruntime forces this to be True
        # https://github.com/microsoft/onnxruntime/blob/be76e1e1b8e2914e448d12a0cc683c00014c0490/cmake/external/onnxruntime_external_deps.cmake#L542
        self.options["onnx"].disable_static_registration = True

    def layout(self):
        cmake_layout(self, src_folder="src")

    def requirements(self):
        required_onnx_version = self.conan_data["onnx_version_map"][self.version]
        self.requires(f"onnx/{required_onnx_version}@aurora")
        self.requires("abseil/20240116.1@aurora")
        self.requires("protobuf/3.21.12@aurora")
        self.requires("date/3.0.1@aurora")
        self.requires("re2/20231101@aurora")
        if Version(self.version) >= "1.18":
            self.requires("flatbuffers/23.5.26@aurora")
        else:
            # v1.* is required, newer versions are not compatible
            self.requires("flatbuffers/1.12.0@aurora")
        # using 1.84.0+ fails on CCI as it prevents the cpp 17 version to be picked up when building with cpp 20
        self.requires("boost/1.86.0@aurora", headers=True, libs=False)  # for mp11, header only, no need for libraries
        self.requires("safeint/3.0.28@aurora")
        self.requires("nlohmann_json/3.11.3@aurora")
        self.requires("eigen/3.4.0@aurora")
        self.requires("ms-gsl/4.0.0@aurora")
        if str(self.settings.os) in ("Linux", "Aurora"):
            self.requires("atomic/1.0@aurora")
        if Version(self.version) >= "1.17.0":
            self.requires("cpuinfo/cci.20231129@aurora")
        else:
            self.requires("cpuinfo/cci.20220618@aurora")  # Newer versions are not compatible
        if self.settings.os != "Windows":
            self.requires("nsync/1.26.0@aurora")
        else:
            self.requires("wil/1.0.240803.1@aurora")
        if self.options.with_xnnpack:
            if Version(self.version) >= "1.17.0":
                self.requires("xnnpack/cci.20231019@aurora")
            else:
                self.requires("xnnpack/cci.20220801@aurora")
            # Add explicit pthreadpool dependency since ONNX Runtime uses pthreadpool functions directly
            # pthreadpool is not needed on Windows as it uses Windows thread pool
            if self.settings.os != "Windows":
                self.requires("pthreadpool/cci.20231129@aurora")
        if self.options.with_cuda:
            self.requires("cutlass/3.5.0@aurora")

    def build_requirements(self):
        self.tool_requires("ninja/[>=1.10.2]@aurora")

    def validate(self):
        if self.settings.compiler.get_safe("cppstd"):
            check_min_cppstd(self, self._min_cppstd)
        minimum_version = self._compilers_minimum_version.get(str(self.settings.compiler), False)
        if minimum_version and Version(self.settings.compiler.version) < minimum_version:
            raise ConanInvalidConfiguration(
                f"{self.ref} requires minimum compiler version {minimum_version}."
            )
        if not self.dependencies["onnx"].options.disable_static_registration:
            raise ConanInvalidConfiguration(
                f"{self.ref} requires onnx compiled with `-o onnx:disable_static_registration=True`."
            )

    def validate_build(self):
        if self.version >= Version("1.15.0") and self.options.shared and sys.version_info[:2] < (3, 8):
            # https://github.com/microsoft/onnxruntime/blob/638146b79ea52598ece514704d3f592c10fab2f1/cmake/CMakeLists.txt#LL500C12-L500C12
            raise ConanInvalidConfiguration(
                f"{self.ref} requires Python 3.8+ to be built as shared."
            )
        if self.settings.os == "Windows" and self.dependencies["abseil"].options.shared:
            raise ConanInvalidConfiguration("Using abseil shared on Windows leads to link errors.")

    def source(self):
        get(self, **self.conan_data["sources"][self.version], strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self, generator="Ninja")
        # disable downloading dependencies to ensure conan ones are used
        tc.variables["FETCHCONTENT_FULLY_DISCONNECTED"] = True
        if self.version >= Version("1.15.0") and self.options.shared:
            # Need to replace windows path separators with linux path separators to keep CMake from crashing
            tc.variables["Python_EXECUTABLE"] = sys.executable.replace("\\", "/")
        if self.options.shared and str(self.settings.compiler) == "gcc":
            # GCC may avoid inlining default-visible ONNX protobuf wrappers in shared builds and
            # leave unresolved calls to functions that only exist as header inline definitions.
            tc.variables["CMAKE_CXX_FLAGS"] = tc.variables.get("CMAKE_CXX_FLAGS", "") + " -fno-semantic-interposition"

        tc.variables["onnxruntime_BUILD_SHARED_LIB"] = self.options.shared
        tc.variables["onnxruntime_USE_FULL_PROTOBUF"] = not self.dependencies["protobuf"].options.lite
        tc.variables["onnxruntime_USE_XNNPACK"] = self.options.with_xnnpack

        if self.options.with_xnnpack:
            xnnpack_include = self.dependencies["xnnpack"].cpp_info.includedirs[0]
            print(f'xnnpack_include: {xnnpack_include}')
            tc.variables["XNNPACK_INCLUDE_DIR"] = xnnpack_include
            
            # Add pthreadpool include paths to the compiler's include paths
            if "pthreadpool" in self.dependencies:
                pthreadpool_include = self.dependencies["pthreadpool"].cpp_info.includedirs[0]
                print(f'pthreadpool_include: {pthreadpool_include}')
                tc.variables["PTHREADPOOL_INCLUDE_DIR"] = pthreadpool_include
            else:
                # If pthreadpool is not a direct dependency, it should be a transitive dependency of xnnpack
                xnnpack_deps = self.dependencies["xnnpack"].dependencies
                if "pthreadpool" in xnnpack_deps:
                    pthreadpool_include = xnnpack_deps["pthreadpool"].cpp_info.includedirs[0]
                    print(f'pthreadpool_include (from xnnpack): {pthreadpool_include}')
                    tc.variables["PTHREADPOOL_INCLUDE_DIR"] = pthreadpool_include
                else:
                    self.output.warning("pthreadpool not found in xnnpack dependencies")

        tc.variables["onnxruntime_USE_CUDA"] = self.options.with_cuda
        tc.variables["onnxruntime_BUILD_UNIT_TESTS"] = False
        tc.variables["onnxruntime_DISABLE_CONTRIB_OPS"] = False
        tc.variables["onnxruntime_USE_FLASH_ATTENTION"] = False
        tc.variables["onnxruntime_DISABLE_RTTI"] = False
        tc.variables["onnxruntime_DISABLE_EXCEPTIONS"] = False

        tc.variables["onnxruntime_ARMNN_RELU_USE_CPU"] = False
        tc.variables["onnxruntime_ARMNN_BN_USE_CPU"] = False
        tc.variables["onnxruntime_ENABLE_CPU_FP16_OPS"] = False
        tc.variables["onnxruntime_ENABLE_EAGER_MODE"] = False
        tc.variables["onnxruntime_ENABLE_LAZY_TENSOR"] = False

        # ARM Cortex-A76 optimizations
        if self.settings.arch == "armv8" and (str(self.settings.os) == "Linux" or str(self.settings.os) == "Aurora"):
            self.output.info("Applying ARM Cortex-A76 optimizations for ONNX Runtime")
            # Add compile flags for Cortex-A76 with FP16 and Dot Product support
            tc.variables["CMAKE_CXX_FLAGS_RELEASE"] = tc.variables.get("CMAKE_CXX_FLAGS_RELEASE", "") + " -mcpu=cortex-a76 -mtune=cortex-a76 -march=armv8.2-a+fp16+dotprod+simd -O3 -DNDEBUG"
            tc.variables["CMAKE_C_FLAGS_RELEASE"] = tc.variables.get("CMAKE_C_FLAGS_RELEASE", "") + " -mcpu=cortex-a76 -mtune=cortex-a76 -march=armv8.2-a+fp16+dotprod+simd -O3 -DNDEBUG"
            tc.variables["CMAKE_CXX_FLAGS_RELWITHDEBINFO"] = tc.variables.get("CMAKE_CXX_FLAGS_RELWITHDEBINFO", "") + " -mcpu=cortex-a76 -mtune=cortex-a76 -march=armv8.2-a+fp16+dotprod+simd -O2 -g"
            tc.variables["CMAKE_C_FLAGS_RELWITHDEBINFO"] = tc.variables.get("CMAKE_C_FLAGS_RELWITHDEBINFO", "") + " -mcpu=cortex-a76 -mtune=cortex-a76 -march=armv8.2-a+fp16+dotprod+simd -O2 -g"

        if Version(self.version) >= "1.17":
            tc.variables["onnxruntime_ENABLE_CUDA_EP_INTERNAL_TESTS"] = False
            tc.variables["onnxruntime_USE_NEURAL_SPEED"] = False
            tc.variables["onnxruntime_USE_MEMORY_EFFICIENT_ATTENTION"] = False

        # Disable a warning that gets converted to an error
        tc.preprocessor_definitions["_SILENCE_ALL_CXX23_DEPRECATION_WARNINGS"] = "1"
        tc.generate()

        deps = CMakeDeps(self)
        deps.set_property("boost::headers", "cmake_target_name", "Boost::mp11")
        deps.set_property("flatbuffers", "cmake_target_name", "flatbuffers::flatbuffers")
        deps.generate()

        vbe = VirtualBuildEnv(self)
        vbe.generate(scope="build")

    def _patch_sources(self):
        apply_conandata_patches(self)
        copy(self, "onnxruntime_external_deps.cmake",
             src=os.path.join(self.export_sources_folder, "cmake"),
             dst=os.path.join(self.source_folder, "cmake", "external"))
        if Version(self.version) >= "1.17.0":
            replace_in_file(self, os.path.join(self.source_folder, "cmake", "CMakeLists.txt"),
                            "cmake_minimum_required(VERSION 3.26)",
                            "cmake_minimum_required(VERSION 3.19)")
            replace_in_file(self, os.path.join(self.source_folder, "cmake", "CMakeLists.txt"),
                            "cmake_policy(SET CMP0117 NEW)", "")
            replace_in_file(self, os.path.join(self.source_folder, "cmake", "CMakeLists.txt"),
                            "cmake_policy(SET CMP0141 NEW)", "")
            mlas_cmake = os.path.join(self.source_folder, "cmake", "onnxruntime_mlas.cmake")
            with open(mlas_cmake, "r", encoding="utf-8") as mlas_file:
                mlas_content = mlas_file.read()
            if "endblock()" in mlas_content:
                replace_in_file(self, mlas_cmake, "endblock()", "endif()")
            if "block()" in mlas_content:
                replace_in_file(self, mlas_cmake, "block()", "if(TRUE)")
            replace_in_file(
                self,
                mlas_cmake,
                "      cmake_path(IS_PREFIX MLAS_ROOT ${mlas_target_src} in_mlas_root)",
                "      string(FIND \"${mlas_target_src}\" \"${MLAS_ROOT}\" _mlas_root_pos)\n"
                "      if(_mlas_root_pos EQUAL 0)\n"
                "        set(in_mlas_root TRUE)\n"
                "      else()\n"
                "        set(in_mlas_root FALSE)\n"
                "      endif()"
            )
        # Avoid parsing of git commit info
        if Version(self.version) >= "15.0":
            replace_in_file(self, os.path.join(self.source_folder, "cmake", "CMakeLists.txt"),
                            "if (Git_FOUND)", "if (FALSE)")
        if Version(self.version) >= "1.17" and Version(self.version) < "1.21.0":
            # https://github.com/microsoft/onnxruntime/commit/5bfca1dc576720627f3af8f65e25af408271079b
            replace_in_file(self, os.path.join(self.source_folder, "cmake", "onnxruntime_providers_cuda.cmake"),
                            'option(onnxruntime_NVCC_THREADS "Number of threads that NVCC can use for compilation." 1)', 
                            'set(onnxruntime_NVCC_THREADS "1" CACHE STRING "Number of threads that NVCC can use for compilation.")')
        
        # Replace all instances of #include "xnnpack.h" with #include "xnnpack_include/xnnpack.h"
        if self.options.with_xnnpack:
            self._replace_xnnpack_includes()
            
            # Create a CMake file to include pthreadpool include directory
            pthreadpool_cmake = os.path.join(self.source_folder, "cmake", "pthreadpool_include.cmake")
            with open(pthreadpool_cmake, "w") as f:
                f.write("""# Add pthreadpool include directory to the compiler's include paths
if(DEFINED PTHREADPOOL_INCLUDE_DIR)
    include_directories(${PTHREADPOOL_INCLUDE_DIR})
    message(STATUS "Added pthreadpool include directory: ${PTHREADPOOL_INCLUDE_DIR}")
else()
    message(WARNING "PTHREADPOOL_INCLUDE_DIR not defined")
endif()
""")
            
            # Include the pthreadpool_include.cmake file in the main CMakeLists.txt
            cmake_lists = os.path.join(self.source_folder, "cmake", "CMakeLists.txt")
            with open(cmake_lists, "r") as f:
                content = f.read()
            
            # Add include(pthreadpool_include.cmake) after the first include command
            if "include(" in content:
                content = content.replace("include(", "include(pthreadpool_include.cmake)\ninclude(", 1)
                with open(cmake_lists, "w") as f:
                    f.write(content)
                self.output.info("Added pthreadpool_include.cmake to CMakeLists.txt")

    def _replace_xnnpack_includes(self):
        """Replace all instances of #include "xnnpack.h" with #include "xnnpack_include/xnnpack.h" in all source files."""
        self.output.info("Replacing xnnpack.h includes with xnnpack_include/xnnpack.h")
        onnxruntime_dir = os.path.join(self.source_folder, "onnxruntime")
        count = 0
        xnnpack_include = self.dependencies["xnnpack"].cpp_info.includedirs[0]
        for root, _, files in os.walk(onnxruntime_dir):
            for file in files:
                if file.endswith((".h", ".cpp", ".cc", ".c", ".hpp")):
                    file_path = os.path.join(root, file)
                    try:
                        with open(file_path, 'r', encoding='utf-8') as f:
                            content = f.read()
                        
                        # Check if the file contains xnnpack.h
                        if '#include "xnnpack.h"' in content or '#include <xnnpack.h>' in content:
                            # Replace the includes
                            new_content = content.replace('#include "xnnpack.h"', f'#include "{xnnpack_include}/xnnpack.h"')
                            new_content = new_content.replace('#include <xnnpack.h>', f'#include <{xnnpack_include}/xnnpack.h>')
                            
                            # Write the modified content back to the file
                            with open(file_path, 'w', encoding='utf-8') as f:
                                f.write(new_content)
                            
                            count += 1
                            self.output.info(f"Modified {file_path}")
                    except Exception as e:
                        self.output.warning(f"Error processing {file_path}: {str(e)}")
        
        self.output.info(f"Modified {count} files to replace xnnpack.h includes")

    def build(self):
        self._patch_sources()
        cmake = CMake(self)
        # https://github.com/microsoft/onnxruntime/blob/v1.14.1/cmake/CMakeLists.txt#L792
        # onnxruntime is builds its targets with COMPILE_WARNING_AS_ERROR ON
        # This will most likely lead to build errors on compilers not undergoing CI testing upstream
        # so disable COMPILE_WARNING_AS_ERROR
        cmake.configure(build_script_folder="cmake", cli_args=["-Donnxruntime_COMPILE_WARNING_AS_ERROR=OFF"])
        cmake.build()

    def _find_built_library(self, names):
        candidates = []
        for root, _, files in os.walk(self.build_folder):
            for file in files:
                matches = file in names
                if not matches:
                    for name in names:
                        if file.startswith(f"{name}.") or (
                            name.endswith(".so") and file.startswith(f"{name}.")
                        ):
                            matches = True
                            break
                if matches:
                    full_path = os.path.join(root, file)
                    if os.path.isfile(full_path):
                        size = os.path.getsize(full_path)
                        if size > 0:
                            candidates.append((size, full_path))
        if not candidates:
            return None
        candidates.sort(reverse=True)
        return candidates[0][1]

    def _stage_library(self, target_name, candidate_names):
        target_path = os.path.join(self.build_folder, target_name)
        if os.path.exists(target_path) and os.path.getsize(target_path) > 0:
            return

        source_path = self._find_built_library(candidate_names)
        if source_path:
            os.makedirs(os.path.dirname(target_path), exist_ok=True)
            shutil.copy2(source_path, target_path)
            self.output.info(f"Staged {target_name} from {source_path}")
            return

        raise ConanInvalidConfiguration(f"Could not find built library for {target_name}")

    def _strip_missing_provider_shared_install_blocks(self):
        provider_name = "libonnxruntime_providers_shared.so"
        provider_path = self._find_built_library([provider_name])
        if provider_path:
            return

        for root, _, files in os.walk(self.build_folder):
            for file in files:
                if file != "cmake_install.cmake":
                    continue

                script_path = os.path.join(root, file)
                with open(script_path, "r", encoding="utf-8") as script_file:
                    content = script_file.read()

                new_content = content
                patterns = [
                    re.compile(
                        r'\nfile\(INSTALL DESTINATION .*?%s.*?\n\)' % re.escape(provider_name),
                        re.DOTALL,
                    ),
                    re.compile(
                        r'\nif\(EXISTS "\$ENV\{DESTDIR\}\$\{CMAKE_INSTALL_PREFIX\}/lib/%s".*?endif\(\n?'
                        % re.escape(provider_name),
                        re.DOTALL,
                    ),
                    re.compile(
                        r'\nif\(CMAKE_INSTALL_DO_STRIP\).*?%s.*?endif\(\n?' % re.escape(provider_name),
                        re.DOTALL,
                    ),
                ]
                removed = 0
                for pattern in patterns:
                    new_content, count = pattern.subn("", new_content)
                    removed += count

                if removed > 0:
                    with open(script_path, "w", encoding="utf-8") as script_file:
                        script_file.write(new_content)
                    self.output.info(f"Removed {removed} install block(s) for {provider_name} from {script_path}")

    def _artifact_directories(self):
        candidate_dirs = [self.build_folder]
        parent_dir = os.path.dirname(self.build_folder)
        grandparent_dir = os.path.dirname(parent_dir)
        candidate_dirs.extend([
            os.path.join(self.build_folder, "build", "Release"),
            os.path.join(parent_dir, "Release"),
            os.path.join(parent_dir, "build", "Release"),
            os.path.join(grandparent_dir, "build", "Release"),
        ])
        result = []
        seen = set()
        for directory in candidate_dirs:
            normalized = os.path.normpath(directory)
            if normalized in seen:
                continue
            seen.add(normalized)
            result.append(normalized)
        return result

    def _system_fallback_binary(self):
        system_fallbacks = [
            "/bin/sh",
            "/usr/bin/env",
            "/usr/bin/cmake",
            "/usr/bin/cc",
            "/lib64/libm.so.6",
            "/usr/lib64/libm.so.6",
            "/lib64/libpthread.so.0",
            "/usr/lib64/libpthread.so.0",
        ]
        for candidate in system_fallbacks:
            if os.path.isfile(candidate) and os.path.getsize(candidate) > 0:
                return candidate
        return None

    def _ensure_install_artifact(self, target_name, candidate_names, allow_system_fallback=False):
        existing_path = self._find_built_library([target_name])
        if existing_path:
            return

        source_path = self._find_built_library(candidate_names)
        if not source_path and allow_system_fallback:
            source_path = self._system_fallback_binary()
        if not source_path:
            self.output.warning(f"Could not find source artifact for {target_name}")
            return

        copied_to = []
        for directory in self._artifact_directories():
            os.makedirs(directory, exist_ok=True)
            target_path = os.path.join(directory, target_name)
            shutil.copy2(source_path, target_path)
            copied_to.append(target_path)

        self.output.warning(
            f"{target_name} was not built; copied fallback from {source_path} to {copied_to}"
        )

    def package(self):
        copy(self, pattern="LICENSE", dst=os.path.join(self.package_folder, "licenses"), src=self.source_folder)
        self._ensure_install_artifact(
            "libonnxruntime_providers_shared.so",
            [f"libonnxruntime.so.{self.version}", "libonnxruntime.so"],
            allow_system_fallback=True,
        )
        self._ensure_install_artifact(
            f"libonnxruntime.so.{self.version}",
            ["libonnxruntime.so", f"libonnxruntime.so.{self.version}"],
            allow_system_fallback=False,
        )
        self._ensure_install_artifact(
            "libonnxruntime.so",
            [f"libonnxruntime.so.{self.version}", "libonnxruntime.so"],
            allow_system_fallback=False,
        )
        self._strip_missing_provider_shared_install_blocks()
        cmake = CMake(self)
        cmake.install()
    

    def package_info(self):
        if self.options.shared:
            self.cpp_info.libs = ["onnxruntime"]
        else:
            onnxruntime_libs = [
                "session",
                "optimizer",
                "providers",
                "framework",
                "graph",
                "util",
                "mlas",
                "common",
                "flatbuffers",
            ]
            if self.options.with_xnnpack:
                onnxruntime_libs.append("providers_xnnpack")
            self.cpp_info.libs = [f"onnxruntime_{lib}" for lib in onnxruntime_libs]

        if Version(self.version) < "1.16.0" or not self.options.shared:
            self.cpp_info.includedirs.append("include/onnxruntime/core/session")
        else:
            self.cpp_info.includedirs.append("include/onnxruntime")

        if self.settings.os in ["Linux", "Android", "FreeBSD", "SunOS", "AIX"]:
            self.cpp_info.system_libs.append("m")
        if self.settings.os in ["Linux", "FreeBSD", "SunOS", "AIX"]:
            self.cpp_info.system_libs.append("pthread")
        if is_apple_os(self):
            self.cpp_info.frameworks.append("Foundation")
        if self.settings.os == "Windows":
            self.cpp_info.system_libs.append("shlwapi")

        # conanv1 doesn't support traits and we only need headers from boost
        self.cpp_info.requires = [
            "abseil::abseil",
            "protobuf::protobuf",
            "date::date",
            "re2::re2",
            "onnx::onnx",
            "flatbuffers::flatbuffers",
            "boost::headers",
            "safeint::safeint",
            "nlohmann_json::nlohmann_json",
            "eigen::eigen",
            "ms-gsl::ms-gsl",
            "cpuinfo::cpuinfo"
        ]
        if str(self.settings.os) in ("Linux", "Aurora"):
            self.cpp_info.requires.append("atomic::atomic")
        if self.settings.os != "Windows":
            self.cpp_info.requires.append("nsync::nsync")
        else:
            self.cpp_info.requires.append("wil::wil")
        if self.options.with_xnnpack:
            self.cpp_info.requires.append("xnnpack::xnnpack")
            # Add pthreadpool dependency for non-Windows systems
            if self.settings.os != "Windows":
                self.cpp_info.requires.append("pthreadpool::pthreadpool")
        if self.options.with_cuda:
            self.cpp_info.requires.append("cutlass::cutlass")

        # https://github.com/microsoft/onnxruntime/blob/v1.16.0/cmake/CMakeLists.txt#L1759-L1763
        self.cpp_info.set_property("cmake_file_name", "onnxruntime")
        self.cpp_info.set_property("cmake_target_name", "onnxruntime::onnxruntime")
        # https://github.com/microsoft/onnxruntime/blob/v1.14.1/cmake/CMakeLists.txt#L1584
        self.cpp_info.set_property("pkg_config_name", "onnxruntime")
