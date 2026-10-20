set(Z80_TICKS_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../vendor/z88dk"
    CACHE PATH "z88dk checkout with the ticks emulator for z80-run")
set(ticks_dir "${Z80_TICKS_SOURCE_DIR}/src/ticks")

if(NOT EXISTS "${ticks_dir}/ticks.c")
  message(STATUS "z80-run: no z88dk checkout at ${Z80_TICKS_SOURCE_DIR}, "
                 "so z80-run is not built")
  return()
endif()
if(NOT CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  message(STATUS "z80-run: the ticks sources need GCC or Clang, "
                 "so z80-run is not built")
  return()
endif()
find_path(Z80_UTHASH_INCLUDE_DIR uthash.h
  HINTS "${Z80_TICKS_SOURCE_DIR}/ext/uthash/src"
  DOC "Directory of uthash.h, for the ticks sources")
if(NOT Z80_UTHASH_INCLUDE_DIR)
  message(STATUS "z80-run: uthash.h not found, so z80-run is not built. "
                 "Run 'git submodule update --init ext/uthash' in "
                 "${Z80_TICKS_SOURCE_DIR}")
  return()
endif()

set(ticks_sources TicksGlue.c)
foreach(cpu zilog gbz80 z180 z80n ez80 r800 rxk r4k i8085 vm1 kc160)
  list(APPEND ticks_sources "${ticks_dir}/${cpu}_inst.c")
endforeach()

# The core is built at -O3 for every LLVM build type, with the warnings of the
# z88dk sources off.
function(add_z80_ticks_library name)
  add_library(${name} STATIC EXCLUDE_FROM_ALL ${ticks_sources})
  target_include_directories(${name} PRIVATE
    "${ticks_dir}" "${Z80_UTHASH_INCLUDE_DIR}")
  target_compile_options(${name} PRIVATE -O3 -w -fvisibility=hidden ${ARGN})
  # A profile records the source path of each function, and ccache rewrites
  # the path, so the core is compiled without the launcher.
  set_property(TARGET ${name} PROPERTY RULE_LAUNCH_COMPILE "")
  set_property(TARGET ${name} PROPERTY C_COMPILER_LAUNCHER "")
endfunction()

set(LLVM_LINK_COMPONENTS Object Support)
set(z80_run_sources z80-run.cpp Loader.cpp)
set(LLVM_OPTIONAL_SOURCES TicksGlue.c)

# The profile format depends on the host compiler, so the profile is made at
# build time and never stored.
option(Z80_RUN_PGO
  "Optimize the z80-run emulator core with a profile of the training corpus"
  ON)
set(pgo_dir "${CMAKE_CURRENT_SOURCE_DIR}/pgo")
set(pgo_method "")
set(corpus_files "")
if(EXISTS "${pgo_dir}/corpus.txt")
  set_property(DIRECTORY APPEND PROPERTY
    CMAKE_CONFIGURE_DEPENDS "${pgo_dir}/corpus.txt")
  file(STRINGS "${pgo_dir}/corpus.txt" corpus_lines REGEX "^[^#]")
  foreach(line ${corpus_lines})
    string(REGEX MATCH "^[^ ]+" file "${line}")
    list(APPEND corpus_files "${pgo_dir}/${file}")
  endforeach()
endif()
set(corpus_complete ON)
foreach(file ${corpus_files})
  if(NOT EXISTS "${file}")
    set(corpus_complete OFF)
  endif()
endforeach()
if(Z80_RUN_PGO)
  if(NOT corpus_files OR NOT corpus_complete)
    message(STATUS "z80-run: the training corpus is incomplete, so PGO is "
                   "off. pgo/build-corpus.sh rebuilds the corpus")
  elseif(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    if(CMAKE_C_COMPILER_VERSION VERSION_LESS 11)
      message(STATUS "z80-run: PGO needs GCC 11 or later, so PGO is off")
    else()
      set(pgo_method gcc)
    endif()
  else()
    get_filename_component(cc_dir "${CMAKE_C_COMPILER}" DIRECTORY)
    string(REGEX MATCH "^[0-9]+" cc_major "${CMAKE_C_COMPILER_VERSION}")
    find_program(Z80_RUN_LLVM_PROFDATA
      NAMES llvm-profdata-${cc_major} llvm-profdata
      HINTS "${cc_dir}"
      DOC "llvm-profdata matching the host C compiler, for z80-run's PGO")
    if(Z80_RUN_LLVM_PROFDATA)
      set(pgo_method clang)
    else()
      message(STATUS "z80-run: no llvm-profdata for the host compiler, "
                     "so PGO is off")
    endif()
  endif()
endif()

if(NOT pgo_method)
  add_z80_ticks_library(Z80Ticks)
else()
  set(gcda_dir "${CMAKE_CURRENT_BINARY_DIR}/pgo-gcda")
  set(profdata "${CMAKE_CURRENT_BINARY_DIR}/z80-run.profdata")
  if(pgo_method STREQUAL "gcc")
    # GCC names a profile after the object file and mixes that name into the
    # identity of static functions. Both builds of the core take the same
    # -dumpbase, so the optimized core finds the profile of the instrumented
    # core.
    foreach(src ${ticks_sources})
      get_filename_component(base "${src}" NAME)
      set_property(SOURCE "${src}" APPEND PROPERTY
        COMPILE_OPTIONS -dumpbase ${base})
    endforeach()
    set(instrument -fprofile-generate -dumpdir "${gcda_dir}/")
    set(use_profile -fprofile-use -fprofile-partial-training
                    -dumpdir "${gcda_dir}/")
  else()
    set(instrument -fprofile-instr-generate)
    set(use_profile -fprofile-instr-use=${profdata})
  endif()
  add_z80_ticks_library(Z80TicksInstr ${instrument})
  add_llvm_executable(z80-run-instr ${z80_run_sources})
  set_target_properties(z80-run-instr PROPERTIES EXCLUDE_FROM_ALL ON)
  target_link_libraries(z80-run-instr PRIVATE Z80TicksInstr)
  target_link_options(z80-run-instr PRIVATE ${instrument})

  # Every training run rewrites this header, and the core includes the header,
  # so the core is rebuilt whenever the profile changes.
  set(pgo_stamp "${CMAKE_CURRENT_BINARY_DIR}/pgo-profile.h")
  add_custom_command(
    OUTPUT "${pgo_stamp}"
    COMMAND "${CMAKE_COMMAND}"
      -DMETHOD=${pgo_method}
      -DRUNNER=$<TARGET_FILE:z80-run-instr>
      -DCORPUS_DIR=${pgo_dir}
      -DGCDA_DIR=${gcda_dir}
      -DPROFRAW_DIR=${CMAKE_CURRENT_BINARY_DIR}/profraw
      -DPROFDATA_TOOL=${Z80_RUN_LLVM_PROFDATA}
      -DPROFDATA=${profdata}
      -DSTAMP=${pgo_stamp}
      -P "${CMAKE_CURRENT_SOURCE_DIR}/PGOTrain.cmake"
    DEPENDS z80-run-instr "${pgo_dir}/corpus.txt" ${corpus_files}
            "${CMAKE_CURRENT_SOURCE_DIR}/PGOTrain.cmake"
    COMMENT "Training z80-run on its PGO corpus"
    VERBATIM)
  add_custom_target(z80-run-pgo-profile DEPENDS "${pgo_stamp}")

  add_z80_ticks_library(Z80Ticks ${use_profile} -include "${pgo_stamp}")
  add_dependencies(Z80Ticks z80-run-pgo-profile)
endif()

add_llvm_utility(z80-run ${z80_run_sources})
target_link_libraries(z80-run PRIVATE Z80Ticks)
