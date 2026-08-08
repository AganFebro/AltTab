cmake_minimum_required(VERSION 3.29)

project(AltTab VERSION 2026.2.0.0 LANGUAGES CXX)

cmake_policy(SET CMP0091 NEW)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

if(NOT WIN32)
    message(FATAL_ERROR "AltTab is a Windows-only application.")
endif()

include(CTest)
enable_language(RC)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(AltTabToolchainChecks)

option(ALTTAB_ENABLE_LOGGER "Build AltTab with log4cpp logging enabled" OFF)
option(ALTTAB_BUILD_TESTER "Build the optional AltTabTester executable" OFF)

set(ALTTAB_GENERATED_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(ALTTAB_GENERATED_VERSION_HEADER "${ALTTAB_GENERATED_INCLUDE_DIR}/version.h")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.h.in"
    "${ALTTAB_GENERATED_VERSION_HEADER}"
    @ONLY
)

set(ALTTAB_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/source")
set(ALTTAB_COMMON_SOURCES
    "${ALTTAB_SOURCE_DIR}/AltTabSettings.cpp"
    "${ALTTAB_SOURCE_DIR}/AltTabTheme.cpp"
    "${ALTTAB_SOURCE_DIR}/AltTabMenu.cpp"
    "${ALTTAB_SOURCE_DIR}/AltTabWindow.cpp"
    "${ALTTAB_SOURCE_DIR}/AltTabWindowRenderer.cpp"
    "${ALTTAB_SOURCE_DIR}/CheckForUpdates.cpp"
    "${ALTTAB_SOURCE_DIR}/fuzzywuzzy.cpp"
    "${ALTTAB_SOURCE_DIR}/GlobalData.cpp"
    "${ALTTAB_SOURCE_DIR}/Logger.cpp"
    "${ALTTAB_SOURCE_DIR}/Utils.cpp"
)
set(ALTTAB_COMMON_HEADERS
    "${ALTTAB_SOURCE_DIR}/AltTab.h"
    "${ALTTAB_SOURCE_DIR}/AltTabSettings.h"
    "${ALTTAB_SOURCE_DIR}/AltTabTheme.h"
    "${ALTTAB_SOURCE_DIR}/AltTabMenu.h"
    "${ALTTAB_SOURCE_DIR}/AltTabWindow.h"
    "${ALTTAB_SOURCE_DIR}/AltTabWindowRenderer.h"
    "${ALTTAB_SOURCE_DIR}/CheckForUpdates.h"
    "${ALTTAB_SOURCE_DIR}/fuzzywuzzy.h"
    "${ALTTAB_SOURCE_DIR}/GlobalData.h"
    "${ALTTAB_SOURCE_DIR}/Logger.h"
    "${ALTTAB_SOURCE_DIR}/PreCompile.h"
    "${ALTTAB_SOURCE_DIR}/resource.h"
    "${ALTTAB_SOURCE_DIR}/targetver.h"
    "${ALTTAB_SOURCE_DIR}/Tooltips.h"
    "${ALTTAB_SOURCE_DIR}/Utils.h"
)
set(ALTTAB_RESOURCES
    "${ALTTAB_SOURCE_DIR}/AltTab.rc"
    "${ALTTAB_SOURCE_DIR}/Manifest.rc"
    "${ALTTAB_SOURCE_DIR}/Version.rc"
)

set(ALTTAB_WINDOWS_LIBRARIES
    comctl32
    Dwmapi
    Shell32
    Version
    Psapi
    Taskschd
    comsuppw
    comsupp
    Uxtheme
    wininet
    ole32
    oleaut32
    advapi32
)

function(alttab_configure_target target_name)
    target_include_directories(${target_name}
        PRIVATE
            "${ALTTAB_GENERATED_INCLUDE_DIR}"
            "${ALTTAB_SOURCE_DIR}"
    )

    set_target_properties(${target_name} PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )

    target_compile_definitions(${target_name}
        PRIVATE
            UNICODE
            _UNICODE
            _WINDOWS
            _CRT_SECURE_NO_WARNINGS
    )

    target_compile_options(${target_name}
        PRIVATE
            "$<$<COMPILE_LANGUAGE:CXX>:/EHsc>"
            "$<$<COMPILE_LANGUAGE:CXX>:/W4>"
            "$<$<COMPILE_LANGUAGE:CXX>:/permissive->"
    )

    target_link_options(${target_name}
        PRIVATE
            "/MANIFEST:NO"
    )

    # The application sources include version.h by name. Force the generated
    # build-tree header so CMake's project VERSION is the active version.
    target_compile_options(${target_name}
        PRIVATE
            "$<$<COMPILE_LANGUAGE:CXX>:/FI${ALTTAB_GENERATED_VERSION_HEADER}>"
    )

    target_link_libraries(${target_name} PRIVATE ${ALTTAB_WINDOWS_LIBRARIES})

    if(ALTTAB_ENABLE_LOGGER)
        target_compile_definitions(${target_name} PRIVATE _AT_LOGGER)
    endif()
endfunction()

add_executable(AltTab WIN32
    "${ALTTAB_SOURCE_DIR}/AltTab.cpp"
    ${ALTTAB_COMMON_SOURCES}
    ${ALTTAB_COMMON_HEADERS}
    ${ALTTAB_RESOURCES}
)
alttab_configure_target(AltTab)
set_target_properties(AltTab PROPERTIES
    OUTPUT_NAME "AltTab"
    LINKER_TYPE LLD
)

if(ALTTAB_BUILD_TESTER)
    add_executable(AltTabTester
        "${CMAKE_CURRENT_SOURCE_DIR}/AltTabTester/AltTabTester.cpp"
        ${ALTTAB_COMMON_SOURCES}
        ${ALTTAB_COMMON_HEADERS}
        "${CMAKE_CURRENT_SOURCE_DIR}/AltTabTester/AltTabTester.h"
        ${ALTTAB_RESOURCES}
    )
    alttab_configure_target(AltTabTester)
    set_target_properties(AltTabTester PROPERTIES
        OUTPUT_NAME "AltTabTester"
        LINKER_TYPE LLD
        WIN32_EXECUTABLE TRUE
    )
    target_link_options(AltTabTester PRIVATE "/ENTRY:wWinMainCRTStartup")
endif()

if(BUILD_TESTING)
    add_executable(AltTabThemeTests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/AltTabThemeTests.cpp"
        "${ALTTAB_SOURCE_DIR}/AltTabTheme.cpp"
        "${ALTTAB_SOURCE_DIR}/AltTabTheme.h"
    )
    target_include_directories(AltTabThemeTests PRIVATE "${ALTTAB_SOURCE_DIR}")
    target_compile_definitions(AltTabThemeTests PRIVATE UNICODE _UNICODE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(AltTabThemeTests PRIVATE /EHsc /W4 /permissive-)
    target_link_libraries(AltTabThemeTests PRIVATE Dwmapi advapi32 user32 gdi32)
    set_target_properties(AltTabThemeTests PROPERTIES LINKER_TYPE LLD)
    add_test(NAME AltTabThemeTests COMMAND AltTabThemeTests)
endif()

if(ALTTAB_ENABLE_LOGGER)
    include(FetchContent)
    set(CMAKE_INSTALL_DEFAULT_COMPONENT_NAME "AltTabLoggerDependency")
    FetchContent_Declare(log4cpp
        URL "${CMAKE_CURRENT_SOURCE_DIR}/libraries/log4cpp-1.1.4.tar.gz"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(log4cpp)
    set(CMAKE_INSTALL_DEFAULT_COMPONENT_NAME "Unspecified")

    find_path(ALTTAB_LOG4CPP_INCLUDE_DIR
        NAMES log4cpp/Category.hh
        PATHS
            "${log4cpp_SOURCE_DIR}/include"
            "${log4cpp_SOURCE_DIR}/log4cpp/include"
        NO_DEFAULT_PATH
    )
    if(NOT ALTTAB_LOG4CPP_INCLUDE_DIR)
        message(FATAL_ERROR "Could not locate headers from the bundled log4cpp archive.")
    endif()

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(ALTTAB_LOG4CPP_TARGET log4cppD)
    else()
        set(ALTTAB_LOG4CPP_TARGET log4cpp)
    endif()
    if(NOT TARGET ${ALTTAB_LOG4CPP_TARGET})
        message(FATAL_ERROR "The bundled log4cpp target '${ALTTAB_LOG4CPP_TARGET}' was not created.")
    endif()

    # The bundled legacy CMake file adds /NODEFAULTLIB:msvcrt. The selected
    # clang-cl runtime must be allowed to select its normal CRT libraries.
    get_target_property(ALTTAB_LOG4CPP_LINK_FLAGS ${ALTTAB_LOG4CPP_TARGET} LINK_FLAGS)
    if(ALTTAB_LOG4CPP_LINK_FLAGS)
        string(REPLACE "/NODEFAULTLIB:msvcrt" "" ALTTAB_LOG4CPP_LINK_FLAGS
            "${ALTTAB_LOG4CPP_LINK_FLAGS}")
        set_target_properties(${ALTTAB_LOG4CPP_TARGET} PROPERTIES
            LINK_FLAGS "${ALTTAB_LOG4CPP_LINK_FLAGS}"
        )
    endif()

    foreach(target_name IN ITEMS AltTab AltTabTester)
        if(TARGET ${target_name})
            target_include_directories(${target_name} PRIVATE "${ALTTAB_LOG4CPP_INCLUDE_DIR}")
            target_link_libraries(${target_name} PRIVATE ${ALTTAB_LOG4CPP_TARGET} ws2_32)
        endif()
    endforeach()
endif()

install(TARGETS AltTab
    RUNTIME DESTINATION .
)
install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/AltTabSettings.ini"
    "${CMAKE_CURRENT_SOURCE_DIR}/AltTab.chm"
    "${CMAKE_CURRENT_SOURCE_DIR}/ReadMe.txt"
    "${CMAKE_CURRENT_SOURCE_DIR}/ReleaseNotes.txt"
    DESTINATION .
)

set(CPACK_GENERATOR "7Z")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
set(CPACK_PACKAGE_NAME "AltTab")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Releases")
set(CPACK_COMPONENTS_ALL Unspecified)
set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)
if(ALTTAB_ENABLE_LOGGER)
    set(CPACK_PACKAGE_FILE_NAME "AltTab_${PROJECT_VERSION}_ReleaseLogger_x64")
else()
    set(CPACK_PACKAGE_FILE_NAME "AltTab_${PROJECT_VERSION}_x64")
endif()
set(CPACK_ARCHIVE_UNSPECIFIED_FILE_NAME "${CPACK_PACKAGE_FILE_NAME}")
include(CPack)
