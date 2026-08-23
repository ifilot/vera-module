# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/mnt/d/PROGRAMMING/KiCAD/vera-module/build/programmer-firmware-pico2/_deps/picotool-src"
  "/mnt/d/PROGRAMMING/KiCAD/vera-module/build/programmer-firmware-pico2/_deps/picotool-build"
  "/mnt/d/PROGRAMMING/KiCAD/vera-module/build/programmer-firmware-pico2/_deps/picotool-subbuild/picotool-populate-prefix"
  "/mnt/d/PROGRAMMING/KiCAD/vera-module/build/programmer-firmware-pico2/_deps/picotool-subbuild/picotool-populate-prefix/tmp"
  "/mnt/d/PROGRAMMING/KiCAD/vera-module/build/programmer-firmware-pico2/_deps/picotool-subbuild/picotool-populate-prefix/src/picotool-populate-stamp"
  "/mnt/d/PROGRAMMING/KiCAD/vera-module/build/programmer-firmware-pico2/_deps/picotool-subbuild/picotool-populate-prefix/src"
  "/mnt/d/PROGRAMMING/KiCAD/vera-module/build/programmer-firmware-pico2/_deps/picotool-subbuild/picotool-populate-prefix/src/picotool-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/mnt/d/PROGRAMMING/KiCAD/vera-module/build/programmer-firmware-pico2/_deps/picotool-subbuild/picotool-populate-prefix/src/picotool-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/mnt/d/PROGRAMMING/KiCAD/vera-module/build/programmer-firmware-pico2/_deps/picotool-subbuild/picotool-populate-prefix/src/picotool-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
