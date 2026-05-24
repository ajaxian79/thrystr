cmake_minimum_required(VERSION 3.20)

project(thrystr
    VERSION 0.1.0
    LANGUAGES CXX
    DESCRIPTION "Skald/OpenGL/ImGui scalar entropy waveform explorer")

option(THRYSTR_BUILD_APP "Build the OpenGL/ImGui application" ON)
option(THRYSTR_BUILD_TESTS "Build unit tests" ON)
option(THRYSTR_BUILD_FUZZ "Build libFuzzer targets" OFF)
set(THRYSTR_SKALD_SOURCE_DIR "" CACHE PATH "Optional local Skald checkout")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING
        "Build type for single-config generators" FORCE)
endif()

include(FetchContent)

if(THRYSTR_SKALD_SOURCE_DIR)
    set(SKALD_BUILD_TEMPLATE OFF CACHE BOOL "" FORCE)
    add_subdirectory("${THRYSTR_SKALD_SOURCE_DIR}"
                     "${CMAKE_BINARY_DIR}/_deps/skald-build")
    set(THRYSTR_SKALD_FONT_DIR "${THRYSTR_SKALD_SOURCE_DIR}/fonts")
else()
    set(SKALD_BUILD_TEMPLATE OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(skald
        GIT_REPOSITORY https://github.com/ajaxian79/skald.git
        GIT_TAG        73e0887af857b8a9d6c84ffd0fd5119c5e166ec3
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(skald)
    set(THRYSTR_SKALD_FONT_DIR "${skald_SOURCE_DIR}/fonts")
endif()

add_library(thrystr_core STATIC
    src/scalar_analysis.cpp)
target_include_directories(thrystr_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_compile_options(thrystr_core PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>)

if(THRYSTR_BUILD_APP)
    find_package(OpenGL REQUIRED)
    find_package(X11 QUIET)

    find_package(glfw3 QUIET)
    if(NOT glfw3_FOUND AND NOT TARGET glfw)
        set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(glfw
            GIT_REPOSITORY https://github.com/glfw/glfw.git
            GIT_TAG        3.4
            GIT_SHALLOW    TRUE)
        FetchContent_MakeAvailable(glfw)
    endif()

    if(NOT TARGET imgui_backends)
        if(NOT DEFINED imgui_SOURCE_DIR)
            get_target_property(THRYSTR_IMGUI_INCLUDE_DIRS imgui INTERFACE_INCLUDE_DIRECTORIES)
            foreach(dir IN LISTS THRYSTR_IMGUI_INCLUDE_DIRS)
                if(EXISTS "${dir}/backends/imgui_impl_glfw.cpp")
                    set(imgui_SOURCE_DIR "${dir}")
                    break()
                endif()
            endforeach()
        endif()
        if(NOT DEFINED imgui_SOURCE_DIR)
            message(FATAL_ERROR "Could not locate Dear ImGui backend sources")
        endif()

        add_library(imgui_backends STATIC
            ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
            ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)
        target_include_directories(imgui_backends SYSTEM PUBLIC
            ${imgui_SOURCE_DIR}/backends)
        target_link_libraries(imgui_backends PUBLIC imgui glfw OpenGL::GL)
    endif()

    add_library(thrystr_stb_image_write STATIC
        third_party/stb/stb_image_impl.cpp
        third_party/stb/stb_image_write_impl.cpp)
    target_include_directories(thrystr_stb_image_write PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb)

    add_executable(thrystr
        src/main.cpp
        src/render_io.cpp)
    target_link_libraries(thrystr PRIVATE
        thrystr_core
        skald::skald
        imgui_backends
        thrystr_stb_image_write
        glfw
        OpenGL::GL)
    if(X11_FOUND)
        target_include_directories(thrystr PRIVATE ${X11_INCLUDE_DIR})
        target_link_libraries(thrystr PRIVATE ${X11_LIBRARIES})
        target_compile_definitions(thrystr PRIVATE THRYSTR_HAS_X11=1)
    endif()
    target_compile_definitions(thrystr PRIVATE
        THRYSTR_SKALD_FONT_DIR="${THRYSTR_SKALD_FONT_DIR}"
        THRYSTR_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")
    target_compile_options(thrystr PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>)
endif()

if(THRYSTR_BUILD_TESTS)
    enable_testing()
    add_executable(thrystr_core_tests
        tests/scalar_analysis_tests.cpp)
    target_link_libraries(thrystr_core_tests PRIVATE thrystr_core)
    add_test(NAME thrystr_core_tests COMMAND thrystr_core_tests)
endif()

if(THRYSTR_BUILD_FUZZ)
    add_executable(thrystr_wave_settings_fuzz
        tests/fuzz/fuzz_wave_settings.cpp)
    target_compile_features(thrystr_wave_settings_fuzz PRIVATE cxx_std_20)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(thrystr_wave_settings_fuzz PRIVATE
            -fsanitize=fuzzer,address,undefined)
        target_link_options(thrystr_wave_settings_fuzz PRIVATE
            -fsanitize=fuzzer,address,undefined)
    endif()
endif()
