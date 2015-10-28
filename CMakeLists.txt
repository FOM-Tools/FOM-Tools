cmake_minimum_required (VERSION 2.8)
project (FOMTools)

set(FOMTOOLS_MAJOR_VERSION 0)
set(FOMTOOLS_MINOR_VERSION 0)
set(FOMTOOLS_PATCH_VERSION 1)
set(FOMTOOLS_VERSION ${FOMTOOLS_MAJOR_VERSION}.${FOMTOOLS_MINOR_VERSION}.${FOMTOOLS_PATCH_VERSION} )

#--- enable unit testing capabilities ------------------------------------------
include(CTest)

#--- massage configure options -------------------------------------------------
add_definitions(--std=c++11)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -Wall")
set(CMAKE_VERBOSE_MAKEFILE ON)

if( "${CMAKE_BUILD_TYPE}" STREQUAL "" )
  set(CMAKE_BUILD_TYPE "RelWithDebInfo")
endif()
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
  set(COMP "llvm")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
  set(COMP "gcc")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
  set(COMP "icc")
endif()

if ( "$ENV{CMTCONFIG}" STREQUAL "")
  if ( "${CMAKE_BUILD_TYPE}" STREQUAL "Debug" )
    set(REL "dbg")
  else()
    set(REL "opt")
  endif()
  execute_process(COMMAND ${CMAKE_C_COMPILER} -dumpversion OUTPUT_VARIABLE COMP_VERSION)
  string(REGEX MATCHALL "[0-9]+" COMP_VERSION_COMPONENTS ${COMP_VERSION})
  list(GET COMP_VERSION_COMPONENTS 0 COMP_MAJOR)
  list(GET COMP_VERSION_COMPONENTS 1 COMP_MINOR)
  set(CMTCONFIG "${CMAKE_SYSTEM_PROCESSOR}-slc6-${COMP}${COMP_MAJOR}${COMP_MINOR}-${REL}")
else("$ENV{CMTCONFIG}" STREQUAL "")
  set(CMTCONFIG $ENV{CMTCONFIG})
endif("$ENV{CMTCONFIG}" STREQUAL "")

#--- dump configure options ----------------------------------------------------
macro (dump_var arg )
  message("${arg}=${${arg}}")
endmacro (dump_var)
dump_var("CMAKE_BUILD_TYPE")
dump_var("CMAKE_CXX_FLAGS")
dump_var("CMAKE_GENERATOR_CXX")
dump_var("CMAKE_CXX_COMPILER")
dump_var("CMAKE_HOST_UNIX")
dump_var("CMAKE_SYSTEM")
dump_var("CMAKE_SYSTEM_NAME")
dump_var("CMAKE_SYSTEM_PROCESSOR")
dump_var("CMAKE_SYSTEM_VERSION")
dump_var("CMAKE_BUILD_TYPE")
dump_var("CMAKE_CXX_COMPILER_ID")
dump_var("CMAKE_CXX_COMPILER_ID_RUN")
dump_var("COMP_VERSION")
dump_var("CMTCONFIG")
dump_var("CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT")
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  message("----------------Default install prefix")
else()
  message("----------------custom install prefix")
  if(NOT DEFINED ENV{CMTCONFIG})
    set (CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}/")
  else()
    set (CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}/${CMTCONFIG}")
  endif()
endif()
dump_var("CMAKE_INSTALL_PREFIX")
dump_var("CMAKE_BINARY_DIR")

#--- find dependencies ---------------------------------------------------------
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake/")
find_package(PythonLibs REQUIRED)
find_package(Unwind REQUIRED)
include_directories(${PYTHON_INCLUDE_DIRS})

#--- add version files ---------------------------------------------------------
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/FOMToolsVersion.h
               ${CMAKE_CURRENT_BINARY_DIR}/FOMToolsVersion.h )
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/FOMToolsVersion.h
        DESTINATION ${CMAKE_INSTALL_PREFIX}/include/FOMTools )

#--- add license files ---------------------------------------------------------
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/COPYING
        DESTINATION ${CMAKE_INSTALL_PREFIX})

include_directories ("${PROJECT_SOURCE_DIR}/" )
SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")


#--- project specific subdirectories -------------------------------------------
add_subdirectory(src)
add_subdirectory(test)

# python wrapper
python_add_module(PageStatusChecker src/PageStatusChecker.cxx)
target_link_libraries(PageStatusChecker rt)

python_add_module(FOMTools FOMTools.cxx)
target_link_libraries(FOMTools FOMUtils)



INSTALL_FILES(/python Monitor.py)
INSTALL_FILES(/python postprocessing/*)
install(TARGETS PageStatusChecker FOMTools DESTINATION "python")
install (DIRECTORY FOMTools/ DESTINATION include
  PATTERN "*.hpp")
include(CMakePackageConfigHelpers)
include(GenerateExportHeader)
set(INCLUDE_INSTALL_DIR "include/")
set(LIB_INSTALL_DIR "lib/")
set(config_install_dir "${LIB_INSTALL_DIR}/cmake/${PROJECT_NAME}")
set(targets_export_name "${PROJECT_NAME}Targets")
set(namespace "${PROJECT_NAME}::")

configure_package_config_file(cmake/FOMToolsConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
  INSTALL_DESTINATION ${config_install_dir}
  PATH_VARS INCLUDE_INSTALL_DIR LIB_INSTALL_DIR)
write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
  VERSION "${FOMTOOLS_VERSION}" COMPATIBILITY SameMajorVersion
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/FOMToolsConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/FOMToolsConfigVersion.cmake
        DESTINATION ${config_install_dir} )
#install(
#    EXPORT "${targets_export_name}"
#    NAMESPACE "${namespace}"
#    DESTINATION "${config_install_dir}"
#)
