# Runs the instrumented z80-run over the training corpus.

foreach(var METHOD RUNNER CORPUS_DIR GCDA_DIR PROFRAW_DIR PROFDATA STAMP)
  if(NOT DEFINED ${var})
    message(FATAL_ERROR "PGOTrain.cmake needs -D${var}")
  endif()
endforeach()

# Counters left by an earlier build of the instrumented core would be merged
# into the new counters or rejected as a mismatch.
if(METHOD STREQUAL "gcc")
  file(GLOB stale "${GCDA_DIR}/*.gcda")
  if(stale)
    file(REMOVE ${stale})
  endif()
else()
  file(REMOVE_RECURSE "${PROFRAW_DIR}")
  set(ENV{LLVM_PROFILE_FILE} "${PROFRAW_DIR}/%p.profraw")
endif()

file(STRINGS "${CORPUS_DIR}/corpus.txt" lines REGEX "^[^#]")
foreach(line ${lines})
  separate_arguments(args UNIX_COMMAND "${line}")
  list(POP_FRONT args program)
  execute_process(
    COMMAND "${RUNNER}" --expect-result=any ${args} "${CORPUS_DIR}/${program}"
    RESULT_VARIABLE status)
  if(NOT status EQUAL 0)
    message(FATAL_ERROR "z80-run training failed on ${program}")
  endif()
endforeach()

if(METHOD STREQUAL "gcc")
  file(GLOB profiles "${GCDA_DIR}/*.gcda")
  if(NOT profiles)
    message(FATAL_ERROR "the training runs left no .gcda files")
  endif()
else()
  file(GLOB raw "${PROFRAW_DIR}/*.profraw")
  if(NOT raw)
    message(FATAL_ERROR "the training runs left no .profraw files")
  endif()
  execute_process(
    COMMAND "${PROFDATA_TOOL}" merge -o "${PROFDATA}" ${raw}
    RESULT_VARIABLE status)
  if(NOT status EQUAL 0)
    message(FATAL_ERROR "llvm-profdata could not merge the training profile")
  endif()
endif()

string(TIMESTAMP now UTC)
file(WRITE "${STAMP}" "/* z80-run PGO profile trained ${now} */\n")
