# Z80Release.cmake
#
# 1-stage Release configuration for the Z80/SM83 cross toolchain.
# Mirrors the quality knobs of the official LLVM release (clang/cmake/caches/
# Release.cmake) but drops the multi-stage PGO/ThinLTO/BOLT machinery, which
# requires a bootstrap. The generated Z80/SM83 code is identical; only clang's
# own speed differs.
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

# Z80/SM83 is registered as an experimental target, so it must be passed via
# LLVM_EXPERIMENTAL_TARGETS_TO_BUILD (not LLVM_TARGETS_TO_BUILD). The host
# (Native) target is kept for build robustness; set LLVM_TARGETS_TO_BUILD to
# "" for a leaner Z80-only package if that builds cleanly in your environment.
set(LLVM_TARGETS_TO_BUILD "Native" CACHE STRING "")
set(LLVM_EXPERIMENTAL_TARGETS_TO_BUILD "Z80" CACHE STRING "")

# Portable binaries: statically link zstd (official does this).
# Drop this line if a static libzstd is not available on the build host.
set(LLVM_USE_STATIC_ZSTD ON CACHE BOOL "")

# Install only the user-facing toolchain, not the LLVM/Clang dev SDK.
# The Z80/SM83 runtime (lib/z80, lib/sm83) is installed via a raw install()
# command in llvm/lib/Target/Z80/CMakeLists.txt and is NOT affected by this.
set(LLVM_INSTALL_TOOLCHAIN_ONLY ON CACHE BOOL "")

# Add llc to the installed toolchain so users can compile .ll directly.
# (toolchain-only otherwise installs only the default tool set, which omits llc.)
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
