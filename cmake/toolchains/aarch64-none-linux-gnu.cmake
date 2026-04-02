set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(AARCH64_TOOLCHAIN_ROOT "${AARCH64_TOOLCHAIN_ROOT}" CACHE PATH
  "Path to the aarch64-none-linux-gnu toolchain root")
set(AARCH64_SYSROOT "${AARCH64_SYSROOT}" CACHE PATH
  "Optional sysroot override for the aarch64-none-linux-gnu toolchain")
set(AARCH64_CXX_INCLUDE_ROOT "${AARCH64_CXX_INCLUDE_ROOT}" CACHE PATH
  "Optional libstdc++ include root override")
set(AARCH64_EXTRA_INCLUDE_DIRS "${AARCH64_EXTRA_INCLUDE_DIRS}" CACHE STRING
  "Semicolon-separated extra include directories for cross-compilation")
set(AARCH64_EXTRA_LIBRARY_DIRS "${AARCH64_EXTRA_LIBRARY_DIRS}" CACHE STRING
  "Semicolon-separated extra library directories for cross-compilation")

if(NOT AARCH64_TOOLCHAIN_ROOT)
  if(DEFINED ENV{AARCH64_TOOLCHAIN_ROOT})
    file(TO_CMAKE_PATH "$ENV{AARCH64_TOOLCHAIN_ROOT}" AARCH64_TOOLCHAIN_ROOT)
  elseif(DEFINED ENV{AARCH64_NONE_LINUX_GNU_ROOT})
    file(TO_CMAKE_PATH "$ENV{AARCH64_NONE_LINUX_GNU_ROOT}" AARCH64_TOOLCHAIN_ROOT)
  endif()
endif()

if(NOT AARCH64_TOOLCHAIN_ROOT)
  message(FATAL_ERROR
    "AARCH64_TOOLCHAIN_ROOT is required. Pass -DAARCH64_TOOLCHAIN_ROOT=/path/to/toolchain.")
endif()

file(TO_CMAKE_PATH "${AARCH64_TOOLCHAIN_ROOT}" AARCH64_TOOLCHAIN_ROOT)

set(_toolchain_bin "${AARCH64_TOOLCHAIN_ROOT}/bin")
set(_toolchain_target_root "${AARCH64_TOOLCHAIN_ROOT}/aarch64-none-linux-gnu")

if(NOT AARCH64_SYSROOT)
  set(AARCH64_SYSROOT "${_toolchain_target_root}/libc")
endif()
file(TO_CMAKE_PATH "${AARCH64_SYSROOT}" AARCH64_SYSROOT)

if(NOT AARCH64_CXX_INCLUDE_ROOT)
  set(AARCH64_CXX_INCLUDE_ROOT "${_toolchain_target_root}/include/c++/10.3.1")
endif()
file(TO_CMAKE_PATH "${AARCH64_CXX_INCLUDE_ROOT}" AARCH64_CXX_INCLUDE_ROOT)

function(_require_tool out_var tool_name)
  if(EXISTS "${_toolchain_bin}/${tool_name}.exe")
    set(${out_var} "${_toolchain_bin}/${tool_name}.exe" PARENT_SCOPE)
  elseif(EXISTS "${_toolchain_bin}/${tool_name}")
    set(${out_var} "${_toolchain_bin}/${tool_name}" PARENT_SCOPE)
  else()
    message(FATAL_ERROR "Could not find ${tool_name} under ${_toolchain_bin}")
  endif()
endfunction()

_require_tool(_gcc gcc)
_require_tool(_gxx g++)
_require_tool(_ar ar)
_require_tool(_ranlib ranlib)
_require_tool(_objdump objdump)

foreach(required_path
    IN ITEMS
      "${AARCH64_SYSROOT}"
      "${AARCH64_CXX_INCLUDE_ROOT}"
      "${AARCH64_CXX_INCLUDE_ROOT}/aarch64-none-linux-gnu"
      "${AARCH64_CXX_INCLUDE_ROOT}/backward")
  if(NOT EXISTS "${required_path}")
    message(FATAL_ERROR "Required cross-compilation path not found: ${required_path}")
  endif()
endforeach()

set(CMAKE_C_COMPILER "${_gcc}")
set(CMAKE_CXX_COMPILER "${_gxx}")
set(CMAKE_AR "${_ar}")
set(CMAKE_RANLIB "${_ranlib}")
set(CMAKE_OBJDUMP "${_objdump}")

set(CMAKE_C_COMPILER_TARGET "aarch64-none-linux-gnu")
set(CMAKE_CXX_COMPILER_TARGET "aarch64-none-linux-gnu")

set(CMAKE_SYSROOT "${AARCH64_SYSROOT}")
set(CMAKE_SYSROOT_COMPILE "${AARCH64_SYSROOT}")
set(CMAKE_SYSROOT_LINK "${AARCH64_SYSROOT}")

set(CMAKE_FIND_ROOT_PATH
  "${AARCH64_SYSROOT}"
  "${_toolchain_target_root}"
)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(CMAKE_C_FLAGS_INIT "--sysroot=${AARCH64_SYSROOT}")
set(CMAKE_CXX_FLAGS_INIT "--sysroot=${AARCH64_SYSROOT}")
set(CMAKE_EXE_LINKER_FLAGS_INIT "--sysroot=${AARCH64_SYSROOT}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "--sysroot=${AARCH64_SYSROOT}")

set(_cxx_include_dirs
  "${AARCH64_CXX_INCLUDE_ROOT}"
  "${AARCH64_CXX_INCLUDE_ROOT}/aarch64-none-linux-gnu"
  "${AARCH64_CXX_INCLUDE_ROOT}/backward"
)

foreach(extra_include_dir IN LISTS AARCH64_EXTRA_INCLUDE_DIRS)
  if(extra_include_dir)
    file(TO_CMAKE_PATH "${extra_include_dir}" extra_include_dir)
    list(APPEND _cxx_include_dirs "${extra_include_dir}")
  endif()
endforeach()

foreach(include_dir IN LISTS _cxx_include_dirs)
  string(APPEND CMAKE_CXX_FLAGS_INIT " -isystem\"${include_dir}\"")
endforeach()

foreach(extra_library_dir IN LISTS AARCH64_EXTRA_LIBRARY_DIRS)
  if(extra_library_dir)
    file(TO_CMAKE_PATH "${extra_library_dir}" extra_library_dir)
    string(APPEND CMAKE_EXE_LINKER_FLAGS_INIT " -L\"${extra_library_dir}\"")
    string(APPEND CMAKE_SHARED_LINKER_FLAGS_INIT " -L\"${extra_library_dir}\"")
  endif()
endforeach()

set(THREADS_PREFER_PTHREAD_FLAG ON)
