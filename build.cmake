
macro (build_zbtunnel SRC_DIR BINARY_DIR)

	set(Boost_USE_STATIC_LIBS TRUE)
	set(Boost_USE_STATIC_RUNTIME TRUE)
	set(Boost_USE_MULTITHREADED TRUE)

	find_package(Boost 1.47.0 COMPONENTS system thread)
	if (NOT Boost_FOUND)
	find_package(Boost REQUIRED COMPONENTS system thread)
	endif ()

	if (WITH_OPENSSL)
	find_package(OpenSSL)
	if (NOT OpenSSL_FOUND)
	find_package(OpenSSL REQUIRED)
	endif ()
	endif ()

	if (MSVC)
		set(CMAKE_CXX_FLAGS "/GF /DWIN32 /W3 /wd4996 /nologo /EHsc /wd4290 /DUNICODE /D_UNICODE /D_WINDOWS /Zm256")
		set(CMAKE_CXX_FLAGS_DEBUG "/Od /MTd /Zi /RTC1 /Gm")
		set(CMAKE_CXX_FLAGS_RELEASE " /GL /MT /Ox /DNDEBUG ")
		set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /LTCG")
		set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG")
	endif ()

        if (UNIX AND NOT CMAKE_SYSTEM_NAME STREQUAL "Darwin") 
            	set(CMAKE_CXX_FLAGS "-std=c++0x")
		set(CMAKE_EXE_LINKER_FLAGS "-lrt")
	endif ()

        if (MSVC)
            set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /DDEBUG /D_DEBUG")
        else ()
            set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D DEBUG -D _DEBUG")
        endif ()

	file (GLOB DLL RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
		"${SRC_DIR}/zbtunnel/[^.]*.cpp"
		"${SRC_DIR}/zbtunnel/[^.]*.hpp"
		"${SRC_DIR}/zbtunnel/[^.]*.h"
		"${SRC_DIR}/zbtunnel/[^.]*.in"
		)

	configure_file("${SRC_DIR}/zbtunnel/zbconfig_inc.hpp.in" "${BINARY_DIR}/gen/zbtunnel/zbconfig_inc.hpp")
	source_group(GENERATED FILES "${BINARY_DIR}/gen/zbtunnel/zbconfig_inc.hpp")
		
	list(REMOVE_ITEM DLL "${SRC_DIR}/zbtunnel/main.cpp")

	include_directories("${SRC_DIR}")
	include_directories("${BINARY_DIR}/gen")
	include_directories(${Boost_INCLUDE_DIRS})
	if (WITH_OPENSSL)
	include_directories("${OPENSSL_INCLUDE_DIR}")
	endif ()

	link_directories(${Boost_LIBRARY_DIRS})

	add_library(zbtunnel_lib ${DLL} "${BINARY_DIR}/gen/zbtunnel/zbconfig_inc.hpp")
	add_executable(zbtunnel "${SRC_DIR}/zbtunnel/main.cpp")
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
endmacro (build_zbtunnel)
