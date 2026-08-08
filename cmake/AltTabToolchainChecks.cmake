if(NOT CMAKE_GENERATOR STREQUAL "Ninja")
    message(FATAL_ERROR
        "AltTab requires the Ninja generator; selected generator: '${CMAKE_GENERATOR}'.")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "AltTab supports x64 Windows only; the selected target is not 64-bit.")
endif()

if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang"
   OR NOT CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC"
   OR NOT CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    message(FATAL_ERROR
        "AltTab requires the clang-cl frontend; selected compiler is '${CMAKE_CXX_COMPILER}'.")
endif()

if(NOT CMAKE_LINKER_TYPE STREQUAL "LLD")
    message(FATAL_ERROR "AltTab requires CMAKE_LINKER_TYPE=LLD.")
endif()

function(alttab_check_llvm_tool variable expected_name)
    if(NOT DEFINED ${variable} OR "${${variable}}" STREQUAL "")
        message(FATAL_ERROR "${variable} must select ${expected_name}.")
    endif()

    get_filename_component(tool_name "${${variable}}" NAME)
    string(TOLOWER "${tool_name}" tool_name)
    string(REGEX REPLACE "\\.exe$" "" tool_name "${tool_name}")
    if(NOT tool_name STREQUAL expected_name)
        message(FATAL_ERROR
            "${variable} must select ${expected_name}; selected '${${variable}}'.")
    endif()

    if(IS_ABSOLUTE "${${variable}}" AND NOT EXISTS "${${variable}}")
        message(FATAL_ERROR "Selected ${expected_name} does not exist: '${${variable}}'.")
    endif()

    message(STATUS "AltTab tool ${expected_name}: ${${variable}}")
endfunction()

alttab_check_llvm_tool(CMAKE_CXX_COMPILER clang-cl)
alttab_check_llvm_tool(CMAKE_LINKER lld-link)
alttab_check_llvm_tool(CMAKE_AR llvm-lib)
alttab_check_llvm_tool(CMAKE_RC_COMPILER llvm-rc)
alttab_check_llvm_tool(CMAKE_MT llvm-mt)

message(STATUS "AltTab toolchain: clang-cl + lld-link + LLVM resource/archive tools, x64")
