# Z80Release.cmake
#
# Usage:
#   cmake -G Ninja -S llvm -B build-release -C clang/cmake/caches/Z80Release.cmake
#   ninja -C build-release
#   cmake --install build-release --prefix <staging-dir>

# Release-quality build (same as official final stage).
set(CMAKE_BUILD_TYPE Release CACHE STRING "")
set(LLVM_ENABLE_ASSERTIONS OFF CACHE BOOL "")
set(CMAKE_POSITION_INDEPENDENT_CODE ON CACHE BOOL "")

# Only the tools and target needed for a Z80/SM83 cross compiler.
set(LLVM_ENABLE_PROJECTS "clang;lld" CACHE STRING "")

set(LLVM_TARGETS_TO_BUILD "" CACHE STRING "")
set(LLVM_EXPERIMENTAL_TARGETS_TO_BUILD "Z80" CACHE STRING "")

# Portable binaries: drop every optional host dependency.
set(LLVM_ENABLE_LIBXML2 OFF CACHE BOOL "")
set(LLVM_ENABLE_ZLIB OFF CACHE BOOL "")
set(LLVM_ENABLE_ZSTD OFF CACHE BOOL "")
set(LLVM_ENABLE_LIBEDIT OFF CACHE BOOL "")

# Install only the user-facing toolchain.
set(LLVM_INSTALL_TOOLCHAIN_ONLY ON CACHE BOOL "")

set(LLVM_TOOLCHAIN_TOOLS
  llc
  llvm-mc
  llvm-ar
  llvm-nm
  llvm-objcopy
  llvm-objdump
  llvm-readobj
  llvm-strip
  CACHE STRING "")
