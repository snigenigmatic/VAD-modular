# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/home/kaustubh/Documents/VAD-modular/cpp_orchestrator/build/_deps/zlib-src")
  file(MAKE_DIRECTORY "/home/kaustubh/Documents/VAD-modular/cpp_orchestrator/build/_deps/zlib-src")
endif()
file(MAKE_DIRECTORY
  "/home/kaustubh/Documents/VAD-modular/cpp_orchestrator/build/_deps/zlib-build"
  "/home/kaustubh/Documents/VAD-modular/cpp_orchestrator/build/_deps/zlib-subbuild/zlib-populate-prefix"
  "/home/kaustubh/Documents/VAD-modular/cpp_orchestrator/build/_deps/zlib-subbuild/zlib-populate-prefix/tmp"
  "/home/kaustubh/Documents/VAD-modular/cpp_orchestrator/build/_deps/zlib-subbuild/zlib-populate-prefix/src/zlib-populate-stamp"
  "/home/kaustubh/Documents/VAD-modular/cpp_orchestrator/build/_deps/zlib-subbuild/zlib-populate-prefix/src"
  "/home/kaustubh/Documents/VAD-modular/cpp_orchestrator/build/_deps/zlib-subbuild/zlib-populate-prefix/src/zlib-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/kaustubh/Documents/VAD-modular/cpp_orchestrator/build/_deps/zlib-subbuild/zlib-populate-prefix/src/zlib-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/kaustubh/Documents/VAD-modular/cpp_orchestrator/build/_deps/zlib-subbuild/zlib-populate-prefix/src/zlib-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
