# Distributed under the OpenDDS License. See accompanying LICENSE
# file or http://www.opendds.org/license.html for details.
#
# This file is setup the way it is for testing purposes. For a cleaner example
# see DevGuideExamples/DCPS/Messenger/CMakeLists.txt

project(opendds_messenger_1 CXX)
cmake_minimum_required(VERSION 3.8.2)
enable_testing()

find_package(OpenDDS REQUIRED)

if (NOT OPENDDS_OWNERSHIP_PROFILE)
  message(FATAL_ERROR "This test requires OpenDDS to be built with Ownership Profile enabled")
endif()

set(target_prefix "${PROJECT_NAME}_")
set(publisher "${target_prefix}publisher")
set(subscriber "${target_prefix}subscriber")

set(src "${CMAKE_CURRENT_SOURCE_DIR}/../../../DCPS/Messenger")
set(dst ${CMAKE_CURRENT_BINARY_DIR})
set(all_targets ${publisher} ${subscriber})

foreach(file
  Messenger.idl subscriber.cpp publisher.cpp Args.h
  DataReaderListener.cpp DataReaderListener.h pub.ini sub.ini)
  configure_file(${src}/${file} ${dst}/${file} COPYONLY)
endforeach()

# Publisher
add_executable(${publisher}
  "${dst}/publisher.cpp"
)
set_target_properties(${publisher}
  PROPERTIES OUTPUT_NAME publisher
)
target_include_directories(${publisher} PRIVATE "${src}/../../../")
OPENDDS_TARGET_SOURCES(${publisher} "${dst}/Messenger.idl")

# Subscriber
add_executable(${subscriber}
  "${dst}/subscriber.cpp"
  "${dst}/DataReaderListener.h"
  "${dst}/DataReaderListener.cpp"
)
set_target_properties(${subscriber}
  PROPERTIES OUTPUT_NAME subscriber
)
OPENDDS_TARGET_SOURCES(${subscriber} "${dst}/Messenger.idl")

foreach(t ${all_targets})
  # Using explicit scope here is a regression test for
  # https://github.com/objectcomputing/OpenDDS/issues/1336
  # See https://cmake.org/cmake/help/latest/policy/CMP0023.html
  target_link_libraries(${t} PUBLIC OpenDDS::OpenDDS)

  # Assert the mapping used was C++03
  get_property(mappings TARGET ${t} PROPERTY OPENDDS_LANGUAGE_MAPPINGS)
  if(NOT "C++03" IN_LIST mappings)
    message(FATAL_ERROR "${t}: C++03 NOT in mapping list: ${mappings}")
  endif()
endforeach()

# Testing
configure_file("${src}/run_test.pl" . COPYONLY)
add_test(NAME "${target_prefix}test"
  COMMAND perl run_test.pl $<$<BOOL:$<CONFIG>>:-ExeSubDir> $<CONFIG>
)
