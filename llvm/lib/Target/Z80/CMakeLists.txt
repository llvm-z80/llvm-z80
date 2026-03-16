add_llvm_component_group(Z80)

set(LLVM_TARGET_DEFINITIONS Z80.td)

tablegen(LLVM Z80GenAsmMatcher.inc -gen-asm-matcher)
tablegen(LLVM Z80GenAsmWriter.inc -gen-asm-writer)
tablegen(LLVM Z80GenCallingConv.inc -gen-callingconv)
tablegen(LLVM Z80GenDAGISel.inc -gen-dag-isel)
tablegen(LLVM Z80GenDisassemblerTables.inc -gen-disassembler)
tablegen(LLVM Z80GenPreLegalizeGICombiner.inc -gen-global-isel-combiner
              -combiners="Z80PreLegalizerCombiner")
tablegen(LLVM Z80GenPostLegalizeGICombiner.inc -gen-global-isel-combiner
              -combiners="Z80PostLegalizerCombiner")
tablegen(LLVM Z80GenGlobalISel.inc -gen-global-isel)
tablegen(LLVM Z80GenInstrInfo.inc -gen-instr-info)
tablegen(LLVM Z80GenMCCodeEmitter.inc -gen-emitter)
tablegen(LLVM Z80GenMCPseudoLowering.inc -gen-pseudo-lowering)
tablegen(LLVM Z80GenRegisterBank.inc -gen-register-bank)
tablegen(LLVM Z80GenRegisterInfo.inc -gen-register-info)
tablegen(LLVM Z80GenSearchableTables.inc -gen-searchable-tables)
tablegen(LLVM Z80GenSubtargetInfo.inc -gen-subtarget)

add_public_tablegen_target(Z80CommonTableGen)

add_llvm_target(Z80CodeGen
  SM83CallLowering.cpp
  Z80AsmPrinter.cpp
  Z80BranchCleanup.cpp
  Z80CallLowering.cpp
  Z80CallingConv.cpp
  Z80PreLegalizerCombiner.cpp
  Z80PostLegalizerCombiner.cpp
  Z80ExpandPseudo.cpp
  Z80FixupImplicitDefs.cpp
  Z80FrameLowering.cpp
  Z80ISelLowering.cpp
  Z80IndexIV.cpp
  Z80InlineAsmLowering.cpp
  Z80InstrCost.cpp
  Z80InstrInfo.cpp
  Z80InstructionSelector.cpp
  Z80LateOptimization.cpp
  Z80LegalizerInfo.cpp
  Z80LowerSelect.cpp
  Z80MCInstLower.cpp
  Z80PostRACompareMerge.cpp
  Z80PostRAScavenging.cpp
  Z80RegisterBankInfo.cpp
  Z80RegisterInfo.cpp
  Z80ShiftRotateChain.cpp
  Z80Subtarget.cpp
  Z80TargetMachine.cpp
  Z80TargetObjectFile.cpp

  DEPENDS
  intrinsics_gen

  LINK_COMPONENTS
  Analysis
  AsmPrinter
  BinaryFormat
  CodeGen
  CodeGenTypes
  Core
  GlobalISel
  IPO
  InstCombine
  LTO
  MC
  Z80Desc
  Z80Info
  Passes
  Scalar
  SelectionDAG
  Support
  Target
  TargetParser
  TransformUtils

  ADD_TO_COMPONENT
  Z80
)

add_subdirectory(AsmParser)
add_subdirectory(Disassembler)
add_subdirectory(MCTargetDesc)
add_subdirectory(TargetInfo)

# Z80/SM83 runtime library build.
# Sources live in compiler-rt/lib/builtins/{z80,sm83}/ following LLVM conventions.
# Two formats are built:
#   ELF:  llvm-mc → .o → llvm-ar → .a  (for integrated assembler + lld)
#   SDCC: sdasz80 → .rel → sdar → .lib (for -fno-integrated-as + sdldz80)
set(Z80_BUILTINS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../compiler-rt/lib/builtins)

function(z80_build_builtins TRIPLE SUBDIR CRT0_ASM RT_DIR SDAS)
  set(OUT ${LLVM_LIBRARY_OUTPUT_INTDIR}/${SUBDIR})
  set(LLVM_MC $<TARGET_FILE:llvm-mc>)
  set(LLVM_AR $<TARGET_FILE:llvm-ar>)

  # --- ELF format (always built) ---
  add_custom_command(
    OUTPUT ${OUT}/${SUBDIR}_crt0.o
    COMMAND ${CMAKE_COMMAND} -E make_directory ${OUT}
    COMMAND ${LLVM_MC} -triple=${TRIPLE} -filetype=obj
            ${CRT0_ASM} -o ${OUT}/${SUBDIR}_crt0.o
    DEPENDS ${CRT0_ASM} llvm-mc
    COMMENT "Assembling ${SUBDIR} crt0 (ELF)"
  )
  list(APPEND ALL_RUNTIME_OUTPUTS ${OUT}/${SUBDIR}_crt0.o)

  file(GLOB RT_SOURCES ${RT_DIR}/*.asm)
  list(FILTER RT_SOURCES EXCLUDE REGEX "crt0.*\\.asm$")
  set(RT_OBJS "")
  foreach(src ${RT_SOURCES})
    get_filename_component(name ${src} NAME_WE)
    set(obj ${OUT}/${name}.o)
    add_custom_command(
      OUTPUT ${obj}
      COMMAND ${LLVM_MC} -triple=${TRIPLE} -filetype=obj ${src} -o ${obj}
      DEPENDS ${src} llvm-mc
      COMMENT "Assembling ${SUBDIR} runtime: ${name}.asm (ELF)"
    )
    list(APPEND RT_OBJS ${obj})
  endforeach()
  add_custom_command(
    OUTPUT ${OUT}/${SUBDIR}_rt.a
    COMMAND ${LLVM_AR} rcs ${OUT}/${SUBDIR}_rt.a ${RT_OBJS}
    DEPENDS ${RT_OBJS} llvm-ar
    COMMENT "Creating ${SUBDIR} runtime archive (ELF)"
  )
  list(APPEND ALL_RUNTIME_OUTPUTS ${OUT}/${SUBDIR}_rt.a)

  set(LDSCRIPT_NAME ${SUBDIR}.ld)
  set(LDSCRIPT ${Z80_BUILTINS_DIR}/${SUBDIR}/${LDSCRIPT_NAME})
  add_custom_command(
    OUTPUT ${OUT}/${LDSCRIPT_NAME}
    COMMAND ${CMAKE_COMMAND} -E copy ${LDSCRIPT} ${OUT}/${LDSCRIPT_NAME}
    DEPENDS ${LDSCRIPT}
    COMMENT "Copying ${SUBDIR} linker script"
  )
  list(APPEND ALL_RUNTIME_OUTPUTS ${OUT}/${LDSCRIPT_NAME})

  install(FILES ${OUT}/${SUBDIR}_crt0.o ${OUT}/${SUBDIR}_rt.a ${OUT}/${LDSCRIPT_NAME}
          DESTINATION lib/${SUBDIR} COMPONENT Z80Runtime)

  # --- SDCC format (optional) ---
  find_program(SDAS_${TRIPLE} ${SDAS})
  find_program(SDAR sdar)
  set(SDAS_PROG ${SDAS_${TRIPLE}})
  if(SDAS_PROG AND SDAR)
    set(CRT0_SDCC_ASM ${RT_DIR}/crt0_sdcc.asm)
    add_custom_command(
      OUTPUT ${OUT}/${SUBDIR}_crt0.rel
      COMMAND ${SDAS_PROG} -g -o ${OUT}/${SUBDIR}_crt0.rel ${CRT0_SDCC_ASM}
      DEPENDS ${CRT0_SDCC_ASM}
      COMMENT "Assembling ${SUBDIR} crt0 (SDCC)"
    )
    list(APPEND ALL_RUNTIME_OUTPUTS ${OUT}/${SUBDIR}_crt0.rel)

    set(RT_RELS "")
    foreach(src ${RT_SOURCES})
      get_filename_component(name ${src} NAME_WE)
      set(rel ${OUT}/${name}.rel)
      add_custom_command(
        OUTPUT ${rel}
        COMMAND ${SDAS_PROG} -g -o ${rel} ${src}
        DEPENDS ${src}
        COMMENT "Assembling ${SUBDIR} runtime: ${name}.asm (SDCC)"
      )
      list(APPEND RT_RELS ${rel})
    endforeach()
    add_custom_command(
      OUTPUT ${OUT}/${SUBDIR}_rt.lib
      COMMAND ${SDAR} rs ${OUT}/${SUBDIR}_rt.lib ${RT_RELS}
      DEPENDS ${RT_RELS}
      COMMENT "Creating ${SUBDIR} runtime archive (SDCC)"
    )
    list(APPEND ALL_RUNTIME_OUTPUTS ${OUT}/${SUBDIR}_rt.lib)

    install(FILES ${OUT}/${SUBDIR}_crt0.rel ${OUT}/${SUBDIR}_rt.lib
            DESTINATION lib/${SUBDIR} COMPONENT Z80Runtime)
  endif()

  set(ALL_RUNTIME_OUTPUTS ${ALL_RUNTIME_OUTPUTS} PARENT_SCOPE)
endfunction()

z80_build_builtins(z80 z80
  ${Z80_BUILTINS_DIR}/z80/crt0.asm
  ${Z80_BUILTINS_DIR}/z80
  sdasz80)

z80_build_builtins(sm83 sm83
  ${Z80_BUILTINS_DIR}/sm83/crt0.asm
  ${Z80_BUILTINS_DIR}/sm83
  sdasgb)

if(ALL_RUNTIME_OUTPUTS)
  add_custom_target(Z80Runtime ALL DEPENDS ${ALL_RUNTIME_OUTPUTS})
endif()
