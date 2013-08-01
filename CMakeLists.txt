cmake_minimum_required (VERSION 2.6)
set (CMAKE_BACKWARDS_COMPATIBILITY 2.6)

Project(zbtunnel)

option(WITH_OPENSSL "Use openssl or not." ON)
if (UNIX)
    option(DISABLE_EPOLL "Disable epoll to support taking file stream as input on posix" OFF)
endif ()
set(ZB_DISPLAY_NAME "ZbTunnel")
set(ZB_VERSION "1.0")

set(BOOST_ROOT "d:/repo/boost")
set(BOOST_LIBRARYDIR "d:/repo/boost/lib")
set(OPENSSL_ROOT_DIR "d:/repo/openssl")

include(build.cmake)
build_zbtunnel("." "${PROJECT_BINARY_DIR}")
if (UNIX)
    install(TARGETS zbtunnel zbtunnel_lib 
        RUNTIME DESTINATION bin 
        ARCHIVE DESTINATION lib
        )
endif ()

