include_directories( ${CMAKE_CURRENT_BINARY_DIR}/..
                     ${CMAKE_CURRENT_SOURCE_DIR}/.. )

add_llvm_component_library(LLVMZ80Info
  Z80TargetInfo.cpp

  LINK_COMPONENTS
  MC
  Support

  ADD_TO_COMPONENT
  Z80
)

add_dependencies(LLVMZ80Info Z80CommonTableGen)
