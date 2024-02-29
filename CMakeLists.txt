cmake_minimum_required(VERSION 3.15)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if (MSVC)
    add_compile_options(/MP)
    add_compile_options("/Zc:__cplusplus")
endif()

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(CMP0091 NEW) # Use modern policy for runtime library selection

enable_language(CXX)
enable_language(C)

find_package (Threads)
find_package(wxWidgets CONFIG REQUIRED) # Used for GUI and HTTPS
find_package(nlohmann_json CONFIG REQUIRED) # Used for JSON parsing/writing


# Library project
project(tattle)


file(GLOB TATTLE_HEADERS src/*.h)
file(GLOB TATTLE_SOURCES src/*.cpp)

add_executable(tattle WIN32 MACOSX_BUNDLE ${TATTLE_HEADERS} ${TATTLE_SOURCES})


if (WIN32)
	#set_property(TARGET tattle PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
	#set(CMAKE_CXX_FLAGS_RELEASE ${CMAKE_CXX_FLAGS_RELEASE} "/MT")
	#set(CMAKE_CXX_FLAGS_DEBUG ${CMAKE_CXX_FLAGS_DEBUG} "/MTd")
	#target_link_libraries(tattle PRIVATE bufferoverflowU)
endif (WIN32)



target_link_libraries(tattle PRIVATE wx::core wx::base wx::net)
target_link_libraries(tattle PUBLIC nlohmann_json::nlohmann_json)

target_include_directories(tattle PUBLIC "thirdparty/include")



# Debugging configuration
set_target_properties(tattle PROPERTIES
	VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/test"
	VS_DEBUGGER_COMMAND_ARGUMENTS "shared_config.json en-US.json error_report.json")
set_target_properties(tattle PROPERTIES 
	XCODE_GENERATE_SCHEME TRUE
	XCODE_SCHEME_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/test"
	XCODE_SCHEME_ARGUMENTS "shared_config.json en-US.json error_report.json")


# Install tattle binary
install(TARGETS tattle DESTINATION bin)


# Solution name
project(tattle)



# Installation
#install(FILES ${TELLING_HEADERS} DESTINATION include)