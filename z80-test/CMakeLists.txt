if(NOT LLVM_MAIN_SRC_DIR)
  message(FATAL_ERROR "z80-test is built as an LLVM external project; "
                      "see z80-test/README.md")
endif()

add_subdirectory(tools/z80-run)

set(Z80_TEST_TARGETS_TO_BUILD
  ${LLVM_TARGETS_TO_BUILD} ${LLVM_EXPERIMENTAL_TARGETS_TO_BUILD})

set(Z80_TOOLS_TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/tools/test)
set(Z80_TOOLS_OBJ_ROOT ${CMAKE_CURRENT_BINARY_DIR}/tools/test)
configure_lit_site_cfg(
  ${Z80_TOOLS_TEST_DIR}/lit.site.cfg.py.in
  ${Z80_TOOLS_OBJ_ROOT}/lit.site.cfg.py
  MAIN_CONFIG
  ${Z80_TOOLS_TEST_DIR}/lit.cfg.py
  PATHS
  "LLVM_TOOLS_DIR"
  "Z80_TOOLS_OBJ_ROOT"
  "Z80_TOOLS_TEST_DIR"
  )

function(z80_variant_flags variant clang_out llc_out)
  set(clang "")
  set(llc "")
  string(REPLACE "+" ";" parts "${variant}")
  foreach(part ${parts})
    if(part STREQUAL "omit-fp")
      list(APPEND clang -fomit-frame-pointer)
    elseif(part STREQUAL "no-static-frames")
      list(APPEND clang -mllvm -z80-static-frames=false)
      list(APPEND llc -z80-static-frames=false)
    elseif(part STREQUAL "inline-rt")
      list(APPEND clang -Xclang -target-feature -Xclang +inline-i16-runtime)
      list(APPEND llc -mattr=+inline-i16-runtime)
    elseif(part STREQUAL "fast-math")
      list(APPEND clang -ffast-math)
    elseif(part STREQUAL "verify")
      list(APPEND clang -mllvm -verify-machineinstrs)
      list(APPEND llc -verify-machineinstrs)
    else()
      message(FATAL_ERROR "unknown Z80 test variant '${part}'")
    endif()
  endforeach()
  string(JOIN " " clang ${clang})
  string(JOIN " " llc ${llc})
  set(${clang_out} "${clang}" PARENT_SCOPE)
  set(${llc_out} "${llc}" PARENT_SCOPE)
endfunction()

function(z80_variant_components cpu opt frame_out other_out)
  set(frame "")
  if(cpu STREQUAL "z80")
    list(APPEND frame omit-fp)
  endif()
  if(NOT opt STREQUAL "O0")
    list(APPEND frame no-static-frames)
  endif()
  set(${frame_out} ${frame} PARENT_SCOPE)
  set(${other_out} inline-rt fast-math PARENT_SCOPE)
endfunction()

function(z80_variant_applies cpu opt variant out)
  z80_variant_components(${cpu} ${opt} parts other)
  list(APPEND parts ${other} verify)
  string(REPLACE "+" ";" wanted "${variant}")
  set(applies TRUE)
  foreach(part ${wanted})
    if(NOT part IN_LIST parts)
      set(applies FALSE)
    endif()
  endforeach()
  set(${out} ${applies} PARENT_SCOPE)
endfunction()

set(Z80_E2E_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/e2e)
function(z80_e2e_config cpu opt variant format out)
  string(JOIN "-" name ${cpu} ${opt} ${variant} ${format})
  set(Z80_E2E_OBJ_ROOT ${CMAKE_CURRENT_BINARY_DIR}/e2e/${name})
  set(Z80_E2E_CPU ${cpu})
  set(Z80_E2E_OPT ${opt})
  set(Z80_E2E_VARIANT ${variant})
  set(Z80_E2E_FORMAT ${format})
  z80_variant_flags("${variant}" Z80_E2E_CLANG_FLAGS Z80_E2E_LLC_FLAGS)
  configure_lit_site_cfg(
    ${Z80_E2E_SOURCE_DIR}/lit.site.cfg.py.in
    ${Z80_E2E_OBJ_ROOT}/lit.site.cfg.py
    MAIN_CONFIG
    ${Z80_E2E_SOURCE_DIR}/lit.cfg.py
    PATHS
    "LLVM_TOOLS_DIR"
    "Z80_E2E_OBJ_ROOT"
    "Z80_E2E_SOURCE_DIR"
    )
  set(${out} ${${out}} ${Z80_E2E_OBJ_ROOT} PARENT_SCOPE)
endfunction()

set(Z80_TORTURE_CONFIG_DIR ${CMAKE_CURRENT_SOURCE_DIR}/torture)
set(Z80_TORTURE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/vendor/gcc-torture)
function(z80_torture_config cpu opt variant out)
  string(JOIN "-" name ${cpu} ${opt} ${variant})
  set(Z80_TORTURE_OBJ_ROOT ${CMAKE_CURRENT_BINARY_DIR}/torture/${name})
  set(Z80_TORTURE_CPU ${cpu})
  set(Z80_TORTURE_OPT ${opt})
  set(Z80_TORTURE_VARIANT ${variant})
  z80_variant_flags("${variant}" Z80_TORTURE_CLANG_FLAGS unused)
  configure_lit_site_cfg(
    ${Z80_TORTURE_CONFIG_DIR}/lit.site.cfg.py.in
    ${Z80_TORTURE_OBJ_ROOT}/lit.site.cfg.py
    MAIN_CONFIG
    ${Z80_TORTURE_CONFIG_DIR}/lit.cfg.py
    PATHS
    "LLVM_TOOLS_DIR"
    "Z80_TORTURE_OBJ_ROOT"
    "Z80_TORTURE_SOURCE_DIR"
    "Z80_TORTURE_CONFIG_DIR"
    )
  set(${out} ${${out}} ${Z80_TORTURE_OBJ_ROOT} PARENT_SCOPE)
endfunction()

# The configurations of check-z80.
set(Z80_E2E_CPUS z80 sm83 CACHE STRING
  "CPUs the end-to-end tests run on")
set(Z80_E2E_OPT_LEVELS O0 O1 O2 O3 Os Oz CACHE STRING
  "Optimization levels the end-to-end tests are compiled at")
set(Z80_E2E_VARIANTS base omit-fp inline-rt CACHE STRING
  "Code generation variants of the end-to-end tests")
set(Z80_E2E_FORMATS elf sdcc CACHE STRING
  "Output formats the end-to-end tests are linked in")
set(Z80_TORTURE_CPUS z80 sm83 CACHE STRING
  "CPUs the GCC torture tests run on")
set(Z80_TORTURE_OPT_LEVELS O2 Os CACHE STRING
  "Optimization levels the GCC torture tests are compiled at")

set(z80_e2e_suites "")
foreach(cpu ${Z80_E2E_CPUS})
  foreach(opt ${Z80_E2E_OPT_LEVELS})
    foreach(variant ${Z80_E2E_VARIANTS})
      if(variant STREQUAL "base")
        set(variant "")
      endif()
      z80_variant_applies(${cpu} ${opt} "${variant}" applies)
      if(NOT applies)
        continue()
      endif()
      foreach(format ${Z80_E2E_FORMATS})
        z80_e2e_config(${cpu} ${opt} "${variant}" ${format} z80_e2e_suites)
      endforeach()
    endforeach()
  endforeach()
endforeach()

set(z80_torture_suites "")
foreach(cpu ${Z80_TORTURE_CPUS})
  foreach(opt ${Z80_TORTURE_OPT_LEVELS})
    z80_torture_config(${cpu} ${opt} "" z80_torture_suites)
  endforeach()
endforeach()

# The configurations of check-z80-all.
set(z80_all_e2e_suites "")
set(z80_all_torture_suites "")
foreach(cpu z80 sm83)
  foreach(opt O0 O1 O2 O3 Os Oz)
    z80_variant_components(${cpu} ${opt} frame other)
    set(variants "")
    list(LENGTH frame count)
    math(EXPR last "(1 << ${count}) - 1")
    foreach(mask RANGE 0 ${last})
      set(variant "")
      set(bit 1)
      foreach(part ${frame})
        math(EXPR selected "${mask} & ${bit}")
        if(NOT selected EQUAL 0)
          list(APPEND variant ${part})
        endif()
        math(EXPR bit "${bit} << 1")
      endforeach()
      list(APPEND variant verify)
      string(JOIN "+" variant ${variant})
      list(APPEND variants ${variant})
    endforeach()
    foreach(part ${other})
      list(APPEND variants ${part}+verify)
    endforeach()
    foreach(variant ${variants})
      foreach(format elf sdcc)
        z80_e2e_config(${cpu} ${opt} ${variant} ${format} z80_all_e2e_suites)
      endforeach()
    endforeach()
    z80_torture_config(${cpu} ${opt} verify z80_all_torture_suites)
    foreach(part ${frame} ${other})
      # The torture tests expect IEEE results.
      if(NOT part STREQUAL "fast-math")
        z80_torture_config(${cpu} ${opt} ${part}+verify z80_all_torture_suites)
      endif()
    endforeach()
  endforeach()
endforeach()

set(z80_lit_paths
  ${LLVM_MAIN_SRC_DIR}/test/CodeGen/Z80
  ${LLVM_MAIN_SRC_DIR}/test/MC/Z80)
# The tools are listed by hand, since the *-test-depends targets also build
# every unit test.
set(z80_test_depends
  FileCheck count not split-file yaml2obj llvm-config llc opt llvm-mc
  llvm-objdump llvm-objcopy llvm-readobj llvm-readelf llvm-ar)
if(TARGET z80-run)
  list(APPEND z80_test_depends z80-run)
endif()
if(TARGET Z80Runtime)
  list(APPEND z80_test_depends Z80Runtime)
endif()
set(clang_test_dir ${LLVM_MAIN_SRC_DIR}/../clang/test)
set(lld_test_dir ${LLVM_MAIN_SRC_DIR}/../lld/test)
if("clang" IN_LIST LLVM_ENABLE_PROJECTS)
  file(GLOB clang_z80_tests CONFIGURE_DEPENDS
    ${clang_test_dir}/*/*z80* ${clang_test_dir}/*/*sm83*)
  # An upstream test that also covers Z80 is listed by hand.
  list(APPEND clang_z80_tests ${clang_test_dir}/Driver/mdouble.c)
  list(REMOVE_DUPLICATES clang_z80_tests)
  list(APPEND z80_lit_paths ${clang_z80_tests})
  list(APPEND z80_test_depends clang)
endif()
if("lld" IN_LIST LLVM_ENABLE_PROJECTS)
  file(GLOB lld_z80_tests CONFIGURE_DEPENDS
    ${lld_test_dir}/ELF/*z80* ${lld_test_dir}/ELF/*sm83*)
  list(REMOVE_DUPLICATES lld_z80_tests)
  list(APPEND z80_lit_paths ${lld_z80_tests})
  list(APPEND z80_test_depends lld)
endif()

add_lit_testsuite(check-z80 "Running the Z80 tests"
  ${z80_lit_paths}
  ${Z80_TOOLS_OBJ_ROOT}
  ${z80_e2e_suites}
  ${z80_torture_suites}
  EXCLUDE_FROM_CHECK_ALL
  DEPENDS ${z80_test_depends}
  )
add_lit_testsuite(check-z80-all "Running every configuration of the Z80 tests"
  ${z80_lit_paths}
  ${Z80_TOOLS_OBJ_ROOT}
  ${z80_all_e2e_suites}
  ${z80_all_torture_suites}
  EXCLUDE_FROM_CHECK_ALL
  DEPENDS ${z80_test_depends}
  )
