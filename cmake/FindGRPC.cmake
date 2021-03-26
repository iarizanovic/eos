#
# Locate and configure the GRPC library
#
# Adds the following targets:
#
#  GRPC::grpc - GRPC library
#  GRPC::grpc++ - GRPC C++ library
#  GRPC::grpc++_reflection - GRPC C++ reflection library
#  GRPC::grpc_cpp_plugin - C++ generator plugin for Protocol Buffers
#

#
# Generates C++ sources from the .proto files
#
# grpc_generate_cpp (<SRCS> <HDRS> <DEST> [<ARGN>...])
#
#  SRCS - variable to define with autogenerated source files
#  HDRS - variable to define with autogenerated header files
#  DEST - directory where the source files will be created
#  ARGN - .proto files
#
function(GRPC_GENERATE_CPP SRCS HDRS DEST)
  if(NOT ARGN)
    message(SEND_ERROR "Error: GRPC_GENERATE_CPP() called without any proto files")
    return()
  endif()

  if(GRPC_GENERATE_CPP_APPEND_PATH)
    # Create an include path for each file specified
    foreach(FIL ${ARGN})
      get_filename_component(ABS_FIL ${FIL} ABSOLUTE)
      get_filename_component(ABS_PATH ${ABS_FIL} PATH)
      list(FIND _protobuf_include_path ${ABS_PATH} _contains_already)
      if(${_contains_already} EQUAL -1)
          list(APPEND _protobuf_include_path -I ${ABS_PATH})
      endif()
    endforeach()
  else()
    set(_protobuf_include_path -I ${CMAKE_CURRENT_SOURCE_DIR})
  endif()

  if(DEFINED PROTOBUF_IMPORT_DIRS)
    foreach(DIR ${PROTOBUF_IMPORT_DIRS})
      get_filename_component(ABS_PATH ${DIR} ABSOLUTE)
      list(FIND _protobuf_include_path ${ABS_PATH} _contains_already)
      if(${_contains_already} EQUAL -1)
          list(APPEND _protobuf_include_path -I ${ABS_PATH})
      endif()
    endforeach()
  endif()

  set(${SRCS})
  set(${HDRS})
  foreach(FIL ${ARGN})
    get_filename_component(ABS_FIL ${FIL} ABSOLUTE)
    get_filename_component(FIL_WE ${FIL} NAME_WE)

    list(APPEND ${SRCS} "${DEST}/${FIL_WE}.grpc.pb.cc")
    list(APPEND ${HDRS} "${DEST}/${FIL_WE}.grpc.pb.h")

    add_custom_command(
      OUTPUT "${DEST}/${FIL_WE}.grpc.pb.cc"
             "${DEST}/${FIL_WE}.grpc.pb.h"
      COMMAND ${CMAKE_COMMAND} -E env
      "LD_LIBRARY_PATH=/opt/eos/grpc/lib64:$LD_LIBRARY_PATH"
      ${PROTOBUF_PROTOC_EXECUTABLE}
      ARGS --grpc_out ${DEST} ${_protobuf_include_path} --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN} ${ABS_FIL}
      DEPENDS ${ABS_FIL} ${PROTOBUF_PROTOC_EXECUTABLE} GRPC::grpc_cpp_plugin
      COMMENT "Running C++ GRPC compiler on ${FIL}"
      VERBATIM )
  endforeach()

  set_source_files_properties(${${SRCS}} ${${HDRS}} PROPERTIES GENERATED TRUE)
  set(${SRCS} ${${SRCS}} PARENT_SCOPE)
  set(${HDRS} ${${HDRS}} PARENT_SCOPE)
endfunction()

# By default have GRPC_GENERATE_CPP macro pass -I to protoc
# for each directory where a proto file is referenced.
if(NOT DEFINED GRPC_GENERATE_CPP_APPEND_PATH)
  set(GRPC_GENERATE_CPP_APPEND_PATH TRUE)
endif()

# Find GRPC include directory
find_path(GRPC_INCLUDE_DIR
  NAMES grpc/grpc.h
  HINTS ${GRPC_ROOT}
  PATHS /opt/eos/grpc/
  PATH_SUFFIXES include
  NO_DEFAULT_PATH)

mark_as_advanced(GRPC_INCLUDE_DIR)

# Find GRPC library
find_library(GRPC_LIBRARY
  NAMES grpc
  HINTS ${GRPC_ROOT}
  PATHS /opt/eos/grpc/
  PATH_SUFFIXES ${CMAKE_INSTALL_LIBDIR}
  NO_DEFAULT_PATH)

# Find GRPC C++ library
find_library(GRPC_GRPC++_LIBRARY
  NAMES grpc++
  HINTS ${GRPC_ROOT}
  PATHS /opt/eos/grpc/
  PATH_SUFFIXES ${CMAKE_INSTALL_LIBDIR}
  NO_DEFAULT_PATH)

# Find GRPC libgpr
find_library(GRPC_LIBGPR_LIBRARY
  NAMES gpr
  HINTS ${GRPC_ROOT}
  PATHS /opt/eos/grpc/
  PATH_SUFFIXES ${CMAKE_INSTALL_LIBDIR}
  NO_DEFAULT_PATH)

# Find GRPC C++ reflection library
find_library(GRPC_GRPC++_REFLECTION_LIBRARY
  NAMES grpc++_reflection
  HINTS ${GRPC_ROOT}
  PATHS /opt/eos/grpc/
  PATH_SUFFIXES ${CMAKE_INSTALL_LIBDIR}
  NO_DEFAULT_PATH)

# Find GRPC CPP generator
find_program(GRPC_CPP_PLUGIN
  NAMES grpc_cpp_plugin
  HINTS ${GRPC_ROOT}
  PATHS /opt/eos/grpc/
  PATH_SUFFIXES bin
  NO_DEFAULT_PATH)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GRPC
  REQUIRED_VARS GRPC_LIBRARY GRPC_INCLUDE_DIR
  GRPC_GRPC++_REFLECTION_LIBRARY GRPC_CPP_PLUGIN)

mark_as_advanced(GRPC_LIBRARY GRPC_GRPC++_LIBRARY
  GRPC_GRPC++_REFLECTION_LIBRARY GRPC_CPP_PLUGIN)

if (GRPC_FOUND AND NOT TARGET GRPC::grpc AND NOT TARGET GRPC::grpc++)
  get_filename_component(GRPC_LD_LIBRARY_PATH ${GRPC_GRPC++_LIBRARY} DIRECTORY)
  message(STATUS "GRPC library link dir: ${GRPC_LD_LIBRARY_PATH}")

  add_library(GRPC::grpc UNKNOWN IMPORTED)
  set_target_properties(GRPC::grpc PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES ${GRPC_INCLUDE_DIR}
    INTERFACE_LINK_LIBRARIES "-lpthread;-ldl"
    IMPORTED_LOCATION ${GRPC_LIBRARY}
    INTERFACE_COMPILE_DEFINITIONS EOS_GRPC=1)

  add_library(GRPC::GPR UNKNOWN IMPORTED)
  set_target_properties(GRPC::GPR PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES ${GRPC_INCLUDE_DIR}
    IMPORTED_LOCATION ${GRPC_LIBGPR_LIBRARY})

  add_library(GRPC::grpc++ UNKNOWN IMPORTED)
  set_target_properties(GRPC::grpc++ PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES ${GRPC_INCLUDE_DIR}
    INTERFACE_LINK_LIBRARIES GRPC::grpc
    INTERFACE_LINK_LIBRARIES GRPC::GPR
    INTERFACE_LINK_DIRECTORIES ${GRPC_LD_LIBRARY_PATH}
    IMPORTED_LOCATION ${GRPC_GRPC++_LIBRARY}
    INTERFACE_COMPILE_DEFINITIONS EOS_GRPC=1)

  add_library(GRPC::grpc++_reflection UNKNOWN IMPORTED)
  set_target_properties(GRPC::grpc++_reflection PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES ${GRPC_INCLUDE_DIR}
    INTERFACE_LINK_LIBRARIES GRPC::grpc++
    IMPORTED_LOCATION ${GRPC_GRPC++_REFLECTION_LIBRARY})

  add_executable(GRPC::grpc_cpp_plugin IMPORTED)
  set_target_properties(GRPC::grpc_cpp_plugin PROPERTIES
    IMPORTED_LOCATION ${GRPC_CPP_PLUGIN})
else()
  message(WARNING "Notice: GRPC not found, no GRPC access available")
  add_library(GRPC::grpc INTERFACE IMPORTED)
  add_library(GRPC::grpc++ INTERFACE IMPORTED)
  add_library(GRPC::grpc++_reflection INTERFACE IMPORTED)
  add_executable(GRPC::grpc_cpp_plugin IMPORTED)
endif()
