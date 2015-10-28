cmake_minimum_required (VERSION 2.8)
project (FOMTools)
set(FOMTOOLS_MAJOR_VERSION 1)
set(FOMTOOLS_MINOR_VERSION 0)
set(FOMTOOLS_PATCH_VERSION 0)
set(FOMTOOLS_VERSION ${FOMTOOLS_MAJOR_VERSION}.${FOMTOOLS_MINOR_VERSION}.${FOMTOOLS_PATCH_VERSION} )

set(targets_export_name "${PROJECT_NAME}Targets")
if( "${CMAKE_BUILD_TYPE}" STREQUAL "" )
  set(CMAKE_BUILD_TYPE "RelWithDebInfo")
endif()
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -Wall")
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
set (CMAKE_VERBOSE_MAKEFILE ON)
include_directories ("${PROJECT_SOURCE_DIR}/" )
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake/")
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
find_package(PythonLibs REQUIRED)
find_package(Unwind REQUIRED)
include_directories(${PYTHON_INCLUDE_DIRS})
python_add_module(PageStatusChecker src/PageStatusChecker.cxx)
target_link_libraries(PageStatusChecker rt)
add_definitions(--std=c++11)
add_library(FOMUtils SHARED src/MergePages.cxx src/Streamers.cxx
  src/RegionFinder.cxx src/Parser.cxx src/Addr2Line.cxx)
add_library(MallocHook SHARED src/mallocinterpose.cxx)
add_executable(binRecord2txt src/MHbin2txt.cxx)
add_executable(dumpFileInfo src/dumpCmdline.cxx)
add_executable(testMallocHook test/test.cxx)
target_link_libraries(binRecord2txt
  FOMUtils
  rt)
target_link_libraries(dumpFileInfo
  FOMUtils
  rt)
target_link_libraries(testMallocHook rt)
target_link_libraries(MallocHook -lunwind FOMUtils dl rt)
target_link_libraries(MallocHook ${UNWIND_LIBRARIES} FOMUtils dl rt)
set_target_properties(MallocHook PROPERTIES LINK_FLAGS "-static-libstdc++ -static-libgcc" )
target_include_directories(MallocHook BEFORE PUBLIC ${UNWIND_INCLUDE_DIRS} )
set_target_properties(FOMUtils PROPERTIES LINK_FLAGS "-static-libstdc++ -static-libgcc" )

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" )
  set_target_properties(FOMUtils PROPERTIES COMPILE_FLAGS "-ftree-vectorize" )
endif()
python_add_module(FOMTools FOMTools.cxx)
target_link_libraries(FOMTools FOMUtils)


install(TARGETS binRecord2txt FOMUtils MallocHook dumpFileInfo testMallocHook
  EXPORT "${targets_export_name}"
  LIBRARY DESTINATION "lib"
  ARCHIVE DESTINATION "lib"
  RUNTIME DESTINATION "bin"
  )
INSTALL(FILES Monitor.py 
  postprocessing/GeneratePlot.py
  postprocessing/PlotHistograms.py
  postprocessing/Postprocessing.py
  postprocessing/Preprocessor.py
  PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE 
  GROUP_READ GROUP_EXECUTE 
  WORLD_READ WORLD_EXECUTE
  DESTINATION "bin")
FILE(WRITE __init__.py "#FOMTools init file")
INSTALL(FILES __init__.py DESTINATION "python/FOMTools" )
install(TARGETS PageStatusChecker FOMTools DESTINATION "python/FOMTools")
install (DIRECTORY FOMTools/ DESTINATION include/FOMTools 
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
install(
    EXPORT "${targets_export_name}"
    NAMESPACE "${namespace}"
    DESTINATION "${config_install_dir}"
)
