cmake_minimum_required(VERSION 3.10)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

#set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(CMP0091 NEW)

enable_language(CXX)
enable_language(C)

find_package (Threads)


# Library project
project(tattle)


file(GLOB TATTLE_HEADERS src/*.h)
file(GLOB TATTLE_SOURCES src/*.cpp)

add_executable(tattle WIN32 MACOSX_BUNDLE ${TATTLE_HEADERS} ${TATTLE_SOURCES})

if (WIN32)
	#set_property(TARGET tattle PROPERTY
	#	MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
		
	#set(CMAKE_CXX_FLAGS_RELEASE ${CMAKE_CXX_FLAGS_RELEASE} "/MT")
	#set(CMAKE_CXX_FLAGS_DEBUG ${CMAKE_CXX_FLAGS_DEBUG} "/MTd")
  
	#target_link_libraries(tattle PRIVATE bufferoverflowU)
endif (WIN32)


#set(wxWidgets_STATIC ON)
#set(VCPKG_TARGET_TRIPLET "x64-windows-static" CACHE STRING "")
#list(APPEND CMAKE_MODULE_PATH "$_VCPKG_ROOT_DIR/ports/wxWidgets/Modules")
find_package(wxWidgets CONFIG REQUIRED)
#target_link_libraries(tattle PUBLIC wxWidgets::wxWidgets)
# target_include_directories(tattle PRIVATE ${wxWidgets_INCLUDE_DIRS})
target_link_libraries(tattle PRIVATE wx::core wx::base wx::net)


# Solution name
project(tattle)



# Installation
#install(TARGETS telling DESTINATION lib)
#install(FILES ${TELLING_HEADERS} DESTINATION include)