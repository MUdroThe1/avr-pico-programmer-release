# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/sebastiaan/Documents/ProtocolDIrectory/pico-sdk/tools/pioasm"
  "/home/sebastiaan/Documents/ProtocolDIrectory/avr-pico-programmer-release/build/pioasm"
  "/home/sebastiaan/Documents/ProtocolDIrectory/avr-pico-programmer-release/build/pioasm-install"
  "/home/sebastiaan/Documents/ProtocolDIrectory/avr-pico-programmer-release/build/pico-sdk/src/rp2_common/pico_cyw43_driver/pioasm/tmp"
  "/home/sebastiaan/Documents/ProtocolDIrectory/avr-pico-programmer-release/build/pico-sdk/src/rp2_common/pico_cyw43_driver/pioasm/src/pioasmBuild-stamp"
  "/home/sebastiaan/Documents/ProtocolDIrectory/avr-pico-programmer-release/build/pico-sdk/src/rp2_common/pico_cyw43_driver/pioasm/src"
  "/home/sebastiaan/Documents/ProtocolDIrectory/avr-pico-programmer-release/build/pico-sdk/src/rp2_common/pico_cyw43_driver/pioasm/src/pioasmBuild-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/sebastiaan/Documents/ProtocolDIrectory/avr-pico-programmer-release/build/pico-sdk/src/rp2_common/pico_cyw43_driver/pioasm/src/pioasmBuild-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/sebastiaan/Documents/ProtocolDIrectory/avr-pico-programmer-release/build/pico-sdk/src/rp2_common/pico_cyw43_driver/pioasm/src/pioasmBuild-stamp${cfgdir}") # cfgdir has leading slash
endif()
