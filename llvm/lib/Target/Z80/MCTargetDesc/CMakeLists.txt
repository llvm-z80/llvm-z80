add_llvm_component_library(LLVMZ80Desc
  Z80AsmBackend.cpp
  Z80ELFObjectWriter.cpp
  Z80InstPrinter.cpp
  Z80MCAsmInfo.cpp
  Z80MCCodeEmitter.cpp
  Z80MCELFStreamer.cpp
  Z80MCExpr.cpp
  Z80MCInstrAnalysis.cpp
  Z80MCTargetDesc.cpp
  Z80TargetStreamer.cpp

  LINK_COMPONENTS
  MC
  Z80Info
  Support

  ADD_TO_COMPONENT
  Z80
)

add_dependencies(LLVMZ80Desc Z80CommonTableGen)
