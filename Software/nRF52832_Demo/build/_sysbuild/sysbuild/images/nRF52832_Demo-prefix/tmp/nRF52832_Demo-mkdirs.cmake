# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "D:/Documents/GitHub/CadenceSensor/Software/nRF52832_Demo")
  file(MAKE_DIRECTORY "D:/Documents/GitHub/CadenceSensor/Software/nRF52832_Demo")
endif()
file(MAKE_DIRECTORY
  "D:/Documents/GitHub/CadenceSensor/Software/nRF52832_Demo/build/nRF52832_Demo"
  "D:/Documents/GitHub/CadenceSensor/Software/nRF52832_Demo/build/_sysbuild/sysbuild/images/nRF52832_Demo-prefix"
  "D:/Documents/GitHub/CadenceSensor/Software/nRF52832_Demo/build/_sysbuild/sysbuild/images/nRF52832_Demo-prefix/tmp"
  "D:/Documents/GitHub/CadenceSensor/Software/nRF52832_Demo/build/_sysbuild/sysbuild/images/nRF52832_Demo-prefix/src/nRF52832_Demo-stamp"
  "D:/Documents/GitHub/CadenceSensor/Software/nRF52832_Demo/build/_sysbuild/sysbuild/images/nRF52832_Demo-prefix/src"
  "D:/Documents/GitHub/CadenceSensor/Software/nRF52832_Demo/build/_sysbuild/sysbuild/images/nRF52832_Demo-prefix/src/nRF52832_Demo-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "D:/Documents/GitHub/CadenceSensor/Software/nRF52832_Demo/build/_sysbuild/sysbuild/images/nRF52832_Demo-prefix/src/nRF52832_Demo-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "D:/Documents/GitHub/CadenceSensor/Software/nRF52832_Demo/build/_sysbuild/sysbuild/images/nRF52832_Demo-prefix/src/nRF52832_Demo-stamp${cfgdir}") # cfgdir has leading slash
endif()
