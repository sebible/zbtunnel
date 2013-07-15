cmake_minimum_required (VERSION 2.6)
set (CMAKE_BACKWARDS_COMPATIBILITY 2.6)

Project(zbtunnel)

option(WITH_OPENSSL "Use openssl or not." ON)
if (UNIX)
option(DISABLE_EPOLL "Disable epoll to support taking file stream as input on posix" OFF)
endif ()
set(ZB_DISPLAY_NAME "ZbTunnel")
set(ZB_VERSION "1.0")

set(Boost_USE_STATIC_LIBS TRUE)
set(Boost_USE_STATIC_RUNTIME TRUE)
set(Boost_USE_MULTITHREADED TRUE)

find_package(Boost 1.47.0 COMPONENTS system thread)
if (NOT Boost_FOUND)
set(BOOST_ROOT "d:/repo/boost")
set(BOOST_LIBRARYDIR "d:/repo/boost/lib")
find_package(Boost REQUIRED COMPONENTS system thread)
endif ()

if (WITH_OPENSSL)
find_package(OpenSSL)
if (NOT OpenSSL_FOUND)
set(OPENSSL_ROOT_DIR "d:/repo/openssl")
find_package(OpenSSL REQUIRED)
endif ()
endif ()

if (MSVC)
	set(CMAKE_CXX_FLAGS "/GF /DWIN32 /W3 /wd4996 /nologo /EHsc /wd4290 /DUNICODE /D_UNICODE /D_WINDOWS /Zm256")
	set(CMAKE_CXX_FLAGS_DEBUG "/Od /MTd /DDEBUG /D_DEBUG /Zi /RTC1 /Gm")
	set(CMAKE_CXX_FLAGS_RELEASE " /GL /MT /Ox /DNDEBUG ")
	#set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /LTCG")
	#set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG")
endif ()

if (UNIX AND NOT CMAKE_SYSTEM_NAME STREQUAL "Darwin") 
    set(CMAKE_CXX_FLAGS "-std=c++0x")
    set(CMAKE_EXE_LINKER_FLAGS "-lrt")
endif ()

file (GLOB DLL RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
	"[^.]*.cpp"
	"[^.]*.hpp"
    "[^.]*.h"
	"[^.]*.in"
    )

configure_file("zbconfig_inc.hpp.in" "${PROJECT_BINARY_DIR}/zbconfig_inc.hpp")
source_group(GENERATED FILES "${PROJECT_BINARY_DIR}/zbconfig_inc.hpp")
	
list(REMOVE_ITEM DLL "main.cpp")

include_directories("${PROJECT_BINARY_DIR}")
include_directories(${Boost_INCLUDE_DIRS})
if (WITH_OPENSSL)
include_directories("${OPENSSL_INCLUDE_DIR}")
endif ()

link_directories(${Boost_LIBRARY_DIRS})

add_library(zbtunnel_lib ${DLL} "${PROJECT_BINARY_DIR}/zbconfig_inc.hpp")
add_executable(zbtunnel "main.cpp")
add_dependencies(zbtunnel zbtunnel_lib)

if (WITH_OPENSSL)
target_link_libraries(zbtunnel_lib ${OPENSSL_LIBRARIES})
endif ()
target_link_libraries(zbtunnel_lib ${Boost_LIBRARIES})
target_link_libraries(zbtunnel zbtunnel_lib)

set_target_properties(zbtunnel PROPERTIES 
	OUTPUTNAME "zbtunnel" 
	)
	
set_target_properties(zbtunnel_lib PROPERTIES 
	OUTPUTNAME "zbtunnel" 
)



