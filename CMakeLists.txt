# SPDX-FileCopyrightText: 2026 Maarten L. Hekkelman
#
# SPDX-License-Identifier: BSL-1.0

cmake_minimum_required(VERSION 3.16)

# set the project name
project(gxrio VERSION 1.3.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(CTest)

set(CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17 CACHE STRING "The minimum version of C++ required for this library")
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_EXAMPLE "Build the my-zcat example application" OFF)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers")
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers")
elseif(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")

    # make msvc standards compliant...
    add_compile_options(/permissive-)

    macro(get_WIN32_WINNT version)
        if(WIN32 AND CMAKE_SYSTEM_VERSION)
            set(ver ${CMAKE_SYSTEM_VERSION})
            string(REPLACE "." "" ver ${ver})
            string(REGEX REPLACE "([0-9])" "0\\1" ver ${ver})

            set(${version} "0x${ver}")
        endif()
    endmacro()

    get_WIN32_WINNT(ver)
    add_definitions(-D_WIN32_WINNT=${ver})
endif()

if(MSVC)
    # Avoid linking the shared library of zlib
    # Search ZLIB_ROOT first if it is set.
    if(ZLIB_ROOT)
        set(_ZLIB_SEARCH_ROOT PATHS ${ZLIB_ROOT} NO_DEFAULT_PATH)
        list(APPEND _ZLIB_SEARCHES _ZLIB_SEARCH_ROOT)
    endif()

    # Normal search.
    set(_ZLIB_x86 "(x86)")
    set(_ZLIB_SEARCH_NORMAL
        PATHS "[HKEY_LOCAL_MACHINE\\SOFTWARE\\GnuWin32\\Zlib;InstallPath]"
        "$ENV{ProgramFiles}/zlib"
        "$ENV{ProgramFiles${_ZLIB_x86}}/zlib")
    unset(_ZLIB_x86)
    list(APPEND _ZLIB_SEARCHES _ZLIB_SEARCH_NORMAL)

    foreach(search ${_ZLIB_SEARCHES})
        find_library(ZLIB_LIBRARY NAMES zlibstatic NAMES_PER_DIR ${${search}} PATH_SUFFIXES lib)
    endforeach()
endif()
find_package(ZLIB REQUIRED)
list(APPEND GXRIO_LIBS ZLIB::ZLIB)

find_package(LibLZMA)
if(LibLZMA_FOUND)
    list(APPEND GXRIO_LIBS LibLZMA::LibLZMA)
else()
    message(WARNING "gxrio: LibLZMA not found, will continue with ZLib only")
endif()

add_library(gxrio INTERFACE)
add_library(gxrio::gxrio ALIAS gxrio)

target_include_directories(gxrio INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

# adding header sources just helps IDEs
target_sources(gxrio INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>$<INSTALL_INTERFACE:include>/gxrio.hpp)

target_link_libraries(gxrio INTERFACE ${GXRIO_LIBS})

set_target_properties(gxrio PROPERTIES PUBLIC_HEADER include/gxrio.hpp)
if(LibLZMA_FOUND)
    target_compile_definitions(gxrio INTERFACE HAVE_LibLZMA=1)
else()
    # Define this explicitly so the header does not auto-detect lzma and then
    # fail to link when CMake did not find the library.
    target_compile_definitions(gxrio INTERFACE HAVE_LibLZMA=0)
endif()

# installation
set(version_config "${CMAKE_CURRENT_BINARY_DIR}/gxrioConfigVersion.cmake")

set(INCLUDE_INSTALL_DIR ${CMAKE_INSTALL_INCLUDEDIR})

include(CMakePackageConfigHelpers)
write_basic_package_version_file("${version_config}"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)

install(TARGETS gxrio
    EXPORT gxrioConfig
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

export(TARGETS gxrio NAMESPACE gxrio:: FILE gxrioTargets.cmake)

if(WIN32 AND NOT CYGWIN)
    set(CONFIG_LOC CMake)
else()
    set(CONFIG_LOC "${CMAKE_INSTALL_LIBDIR}/cmake/gxrio")
endif()

if(LibLZMA_FOUND)
    set(GXRIO_LZMA_DEPENDENCY "find_dependency(LibLZMA REQUIRED)")
else()
    set(GXRIO_LZMA_DEPENDENCY "")
endif()

configure_package_config_file(
    ${PROJECT_SOURCE_DIR}/cmake/gxrioConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/gxrioConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/gxrio
    PATH_VARS INCLUDE_INSTALL_DIR)

install(EXPORT gxrioConfig
    FILE gxrioTargets.cmake
    NAMESPACE gxrio::
    DESTINATION ${CONFIG_LOC})
install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/cmake/gxrioConfig.cmake "${version_config}"
    DESTINATION ${CONFIG_LOC})

if(BUILD_TESTING)
    enable_testing()

    find_package(Catch2 REQUIRED)

    list(APPEND tests unit-test unit-test-gzip)
    if(LibLZMA_FOUND)
        list(APPEND tests unit-test-xz)
    endif()

    foreach(TEST IN LISTS tests)
        set(TEST_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/test/${TEST}.cpp")

        add_executable(${TEST} ${TEST_SOURCE})

        target_link_libraries(${TEST} gxrio::gxrio Catch2::Catch2)

        if(MSVC)
            # Specify unwind semantics so that MSVC knowns how to handle exceptions
            target_compile_options(${TEST} PRIVATE /EHsc)
        endif()

        add_test(NAME ${TEST}
            COMMAND $<TARGET_FILE:${TEST}> --testdir ${PROJECT_SOURCE_DIR}/test)
    endforeach()
endif(BUILD_TESTING)

if(BUILD_EXAMPLE)
    add_executable(my-zcat example/my-zcat.cpp)
    target_link_libraries(my-zcat gxrio::gxrio)
endif()
