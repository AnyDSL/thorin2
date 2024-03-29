cmake_minimum_required(VERSION 3.25 FATAL_ERROR)

project(Thorin VERSION 1.9.0)

include(CMakeDependentOption)
include(CMakePackageConfigHelpers)
include(CMakePrintHelpers)
include(GNUInstallDirs)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
include(Thorin)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

option(BUILD_SHARED_LIBS           "Build shared libraries." ON)
option(THORIN_BUILD_DOCS           "If ON, Thorin will build the documentation (requires Doxygen)." OFF)
option(THORIN_BUILD_EXAMPLES       "If ON, Thorin will build examples." OFF)

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}; shared libs: ${BUILD_SHARED_LIBS}")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    option(THORIN_ENABLE_CHECKS "Enable expensive checks" ON)
else()
    option(THORIN_ENABLE_CHECKS "Enable expensive checks" OFF)
endif()

if(WIN32)
    add_compile_definitions(NOMINMAX) # prevents windows.h defining min/max macros
else()
    add_compile_options(-Wall -Wextra)
endif()

add_custom_target(thorin_all)

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    find_program(MEMORYCHECK_COMMAND valgrind)
    set(VALGRIND_COMMAND_OPTIONS "--leak-check=full --error-exitcode=1")
    option(BUILD_TESTING "Build lit and unit tests." OFF) # needs to be BEFORE "include(CTest)"
    include(CTest)
    cmake_dependent_option(THORIN_LIT_WITH_VALGRIND "If ON, the Thorin CLI in the lit tests will be run under valgrind." OFF "BUILD_TESTING" OFF)
    set(THORIN_LIT_TIMEOUT 10 CACHE STRING "Timout for lit tests.")
    if(BUILD_TESTING)
        include(GoogleTest)
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        add_subdirectory(external/googletest EXCLUDE_FROM_ALL)
        add_subdirectory(gtest)
        add_subdirectory(lit)
    endif()
endif()

set(ABSL_PROPAGATE_CXX_STD ON)
set(ABSL_USE_EXTERNAL_GOOGLETEST ON)
set(ABSL_ENABLE_INSTALL ON)
set(FE_ABSL ON)

add_subdirectory(external/abseil-cpp)
add_subdirectory(external/fe)
add_subdirectory(external/lyra)
add_subdirectory(external/rang)
add_subdirectory(src)

if(THORIN_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(THORIN_BUILD_DOCS)
    find_package(Doxygen REQUIRED dot)
    if(DOXYGEN_FOUND)
        add_subdirectory(docs)
    endif()
endif()

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/thorin-config-version.cmake"
    VERSION ${Thorin_VERSION}
    COMPATIBILITY SameMajorVersion
)
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/thorin-config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/thorin-config.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/thorin"
)
install(
    TARGETS libthorin
    EXPORT thorin-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(
    FILES "${PROJECT_SOURCE_DIR}/external/half/include/half.hpp"
    DESTINATION include
)
install(
    FILES
        cmake/Thorin.cmake
        "${CMAKE_CURRENT_BINARY_DIR}/thorin-config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/thorin-config-version.cmake"
    DESTINATION
        ${CMAKE_INSTALL_LIBDIR}/cmake/thorin
)
install(
    EXPORT thorin-targets
    NAMESPACE thorin::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/thorin
)
