add_llvm_component_library(LLVMZ80AsmParser
  Z80AsmParser.cpp

  LINK_COMPONENTS
  MC
  MCParser
  Z80Desc
  Z80Info
  Support

  ADD_TO_COMPONENT
  Z80
)
