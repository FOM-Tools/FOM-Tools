include_directories(${CMAKE_SOURCE_DIR}/include)

#--- FOMUtils ------------------------------------------------------------------
add_library(FOMUtils SHARED MergePages.cxx Streamers.cxx RegionFinder.cxx
                            Parser.cxx Addr2Line.cxx)
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" )
  set_target_properties(FOMUtils PROPERTIES COMPILE_FLAGS "-ftree-vectorize" )
endif()
set_target_properties(FOMUtils PROPERTIES LINK_FLAGS "-static-libstdc++ -static-libgcc" )


#--- MallocHook ----------------------------------------------------------------
add_library(MallocHook SHARED mallocinterpose.cxx)
target_link_libraries(MallocHook ${UNWIND_LIBRARIES} FOMUtils dl rt)
set_target_properties(MallocHook PROPERTIES LINK_FLAGS "-static-libstdc++ -static-libgcc" )
target_include_directories(MallocHook BEFORE PUBLIC ${UNWIND_INCLUDE_DIRS} )


#--- Executables ---------------------------------------------------------------
add_executable(binRecord2txt MHbin2txt.cxx)
target_link_libraries(binRecord2txt FOMUtils rt)

add_executable(dumpFileInfo dumpCmdline.cxx)
target_link_libraries(dumpFileInfo FOMUtils rt)


#--- Install targets -----------------------------------------------------------
install(TARGETS binRecord2txt FOMUtils MallocHook dumpFileInfo
  EXPORT "${targets_export_name}"
  LIBRARY DESTINATION "lib"
  ARCHIVE DESTINATION "lib"
  RUNTIME DESTINATION "bin"
  )
