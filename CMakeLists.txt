cmake_minimum_required(VERSION 3.16)
project(libwpd LANGUAGES C CXX)

option(LIBWPD_USE_CUSTOM_WINDOWS_HEADERS "Use headers from Microsoft that currently aren't available in MinGW" ON)
option(LIBWPD_BUILD_TEST "" OFF)

set(LIBWPD_CORE src/main.cpp src/parse.c)
set(LIBWPD_FLAGS -Wall -Wshadow -Wcast-qual -Wpedantic -Werror=deprecated-declarations -Wno-unknown-pragmas)

set(LIBWPD_CUSTOM_HEADERS)
if(LIBWPD_USE_CUSTOM_WINDOWS_HEADERS)
	set(LIBWPD_CUSTOM_HEADERS PRIVATE windows/)
endif()

add_library(wpd SHARED)
target_sources(wpd PRIVATE src/dll.cpp ${LIBWPD_CORE})
target_include_directories(wpd PUBLIC ${PROJECT_SOURCE_DIR} ${LIBWPD_CUSTOM_HEADERS})
target_compile_options(wpd PRIVATE ${LIBWPD_FLAGS})
target_link_libraries(wpd PRIVATE ole32 oleaut32 portabledeviceguids)

add_library(wpd_static STATIC)
target_sources(wpd_static PRIVATE ${LIBWPD_CORE})
target_include_directories(wpd_static PUBLIC ${PROJECT_SOURCE_DIR} ${LIBWPD_CUSTOM_HEADERS})
target_compile_options(wpd_static PRIVATE ${LIBWPD_FLAGS})

if(LIBWPD_BUILD_TEST)
	add_executable(test test.c)
	target_link_libraries(test PRIVATE wpd_static ole32 oleaut32 portabledeviceguids)
endif()
