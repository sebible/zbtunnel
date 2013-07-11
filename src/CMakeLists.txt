cmake_minimum_required (VERSION 2.6)
set (CMAKE_BACKWARDS_COMPATIBILITY 2.6)

Project(zbtunnel)

set(Boost_USE_STATIC_LIBS TRUE)

find_package(Boost 1.47.0 COMPONENTS system thread)
if (NOT Boost_FOUND)
set(BOOST_ROOT "d:/repo/boost")
set(BOOST_LIBRARYDIR "d:/repo/boost/lib")
find_package(Boost REQUIRED COMPONENTS system thread)
endif ()

find_package(OpenSSL)
if (NOT OpenSSL_FOUND)
set(OPENSSL_ROOT_DIR "d:/repo/openssl")
find_package(OpenSSL REQUIRED)
endif ()

if (WIN32)
	set(CMAKE_CXX_FLAGS "/GF /DWIN32 /DXP_WIN=1 /W3 /wd4996 /nologo /EHsc /wd4290 /DUNICODE /D_UNICODE /D_WINDOWS /Zm256")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Od /DDEBUG /D_DEBUG /Zi /RTC1 /Gm")
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /GL /Ox /DNDEBUG")
	set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /LTCG ")
	set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG ")
endif ()

if (UNIX AND NOT CMAKE_SYSTEM_NAME STREQUAL "Darwin") 
    set(CMAKE_CXX_FLAGS "-std=c++0x")
    set(CMAKE_EXE_LINKER_FLAGS "-lrt")
endif ()

if (APPLE)
    #set(CMAKE_CXX_FLAGS "")
    #set(CMAKE_EXE_LINKER_FLAGS "-lrt")
endif ()
    


file (GLOB DLL RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
	"[^.]*.cpp"
	"[^.]*.hpp"
    "[^.]*.h"
    )
	
list(REMOVE_ITEM DLL "main.cpp")

include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
include_directories("${OPENSSL_INCLUDE_DIR}")
include_directories(${Boost_INCLUDE_DIRS})

link_directories(${Boost_LIBRARY_DIRS})

add_library(zbtunnel_lib ${DLL})
add_executable(zbtunnel "main.cpp")
add_dependencies(zbtunnel zbtunnel_lib)

target_link_libraries(zbtunnel_lib ${OPENSSL_LIBRARIES})
target_link_libraries(zbtunnel_lib ${Boost_LIBRARIES})
target_link_libraries(zbtunnel zbtunnel_lib)

set_target_properties(zbtunnel PROPERTIES 
	OUTPUTNAME "zbtunnel" 
	)
	
set_target_properties(zbtunnel_lib PROPERTIES 
	OUTPUTNAME "zbtunnel" 
)



