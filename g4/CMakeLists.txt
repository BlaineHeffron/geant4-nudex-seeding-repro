cmake_minimum_required(VERSION 3.16)
project(nudex_seed_g4 CXX)
find_package(Geant4 11.3 REQUIRED)
add_executable(capture_repro capture_repro.cc)
target_link_libraries(capture_repro PRIVATE ${Geant4_LIBRARIES})
target_compile_features(capture_repro PRIVATE cxx_std_17)

# Optional: the NuDEX model from a Geant4 source tree with patches/geant4-nudex-cascade-seed.patch,
# built as a shared library to LD_PRELOAD in front of the installed one (tests the fix without
# rebuilding Geant4). Configure with -DGEANT4_SOURCE_DIR=<geant4 source matching the install>.
set(GEANT4_SOURCE_DIR "" CACHE PATH "Geant4 source tree for the patched-NuDEX test library")
if(GEANT4_SOURCE_DIR)
  find_package(Git REQUIRED)
  set(fix ${CMAKE_BINARY_DIR}/nudex-patched)
  if(NOT EXISTS ${fix}/source)
    file(COPY ${GEANT4_SOURCE_DIR}/source/processes/hadronic/models/nudex
         DESTINATION ${fix}/source/processes/hadronic/models)
    # git init first: inside another checkout, git apply skips paths outside the current directory.
    execute_process(COMMAND ${GIT_EXECUTABLE} init -q WORKING_DIRECTORY ${fix} COMMAND_ERROR_IS_FATAL ANY)
    execute_process(COMMAND ${GIT_EXECUTABLE} apply ${CMAKE_CURRENT_SOURCE_DIR}/../patches/geant4-nudex-cascade-seed.patch
                    WORKING_DIRECTORY ${fix} COMMAND_ERROR_IS_FATAL ANY)
  endif()
  file(STRINGS ${fix}/source/processes/hadronic/models/nudex/src/G4NuDEXNeutronCaptureModel.cc applied REGEX "G4UniformRand\\(\\) \\) \\);")
  if(NOT applied)
    message(FATAL_ERROR "patches/geant4-nudex-cascade-seed.patch did not apply to ${fix}")
  endif()
  set(m ${fix}/source/processes/hadronic/models/nudex)
  file(GLOB fix_src ${m}/src/*.cc)
  add_library(nudex_patched SHARED ${fix_src})
  target_include_directories(nudex_patched PRIVATE ${m}/include ${m}/include/private)
  target_link_libraries(nudex_patched PRIVATE ${Geant4_LIBRARIES})
  target_compile_features(nudex_patched PRIVATE cxx_std_17)
endif()
