add_executable( appbase_example main.cpp )
target_link_libraries( appbase_example appbase ${CMAKE_DL_LIBS} ${PLATFORM_SPECIFIC_LIBS} )
set_target_properties( appbase_example PROPERTIES CXX_STANDARD 14 CXX_EXTENSIONS OFF )