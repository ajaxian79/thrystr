cmake_minimum_required(VERSION 3.20)

project(thrystr
    VERSION 0.1.0
    LANGUAGES CXX
    DESCRIPTION "Skald/OpenGL/ImGui scalar entropy waveform explorer")

option(THRYSTR_BUILD_APP "Build the OpenGL/ImGui application" ON)
option(THRYSTR_BUILD_TESTS "Build unit tests" ON)
option(THRYSTR_BUILD_FUZZ "Build libFuzzer targets" OFF)
option(THRYSTR_BUILD_DOCS "Build the in-house documentation outputs" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING
        "Build type for single-config generators" FORCE)
endif()

function(thrystr_disable_vendor_warnings target_name)
    if(TARGET ${target_name})
        target_compile_options(${target_name} PRIVATE
            $<$<COMPILE_LANG_AND_ID:C,GNU,Clang>:-w>
            $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang>:-w>
            $<$<COMPILE_LANG_AND_ID:C,MSVC>:/w>
            $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/w>)
        set_target_properties(${target_name} PROPERTIES
            C_CLANG_TIDY ""
            CXX_CLANG_TIDY "")
    endif()
endfunction()

add_library(thrystr_core STATIC
    src/scalar_analysis.cpp)
target_include_directories(thrystr_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_compile_options(thrystr_core PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>)

add_library(thrystr_model STATIC
    src/model/dataset.cpp
    src/model/entity.cpp
    src/model/fitter.cpp
    src/model/node_graph.cpp
    src/model/property_bag.cpp
    src/model/standard_math.cpp
    src/model/workspace.cpp)
target_include_directories(thrystr_model PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_compile_options(thrystr_model PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>)

add_library(thrystr_ui STATIC
    src/ui/window.cpp)
target_include_directories(thrystr_ui PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_link_libraries(thrystr_ui PUBLIC thrystr_model)
target_compile_options(thrystr_ui PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>)

add_library(thrystr_app STATIC
    src/app/workspace.cpp
    src/app/fit_validation.cpp
    src/app/convergent_fit.cpp
    src/app/multi_track_fit.cpp)
target_include_directories(thrystr_app PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_link_libraries(thrystr_app PUBLIC thrystr_core thrystr_model thrystr_ui)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_link_libraries(thrystr_app PUBLIC quadmath)
endif()
target_compile_options(thrystr_app PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>)

if(THRYSTR_BUILD_APP)
    find_package(OpenGL REQUIRED)
    find_package(X11 QUIET)

    set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)
    if(UNIX AND NOT APPLE)
        set(GLFW_BUILD_X11     ON  CACHE BOOL "" FORCE)
        set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
    endif()
    add_subdirectory(src/gui/vendor/glfw EXCLUDE_FROM_ALL)
    add_library(thrystr::glfw ALIAS glfw)
    thrystr_disable_vendor_warnings(glfw)

    add_library(thrystr_interface_vendor STATIC
        src/gui/vendor/imgui/imgui.cpp
        src/gui/vendor/imgui/imgui_draw.cpp
        src/gui/vendor/imgui/imgui_tables.cpp
        src/gui/vendor/imgui/imgui_widgets.cpp
        src/gui/vendor/imgui/backends/imgui_impl_glfw.cpp
        src/gui/vendor/imgui/backends/imgui_impl_opengl3.cpp)
    target_include_directories(thrystr_interface_vendor SYSTEM PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gui/vendor/imgui
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gui/vendor/imgui/backends)
    target_link_libraries(thrystr_interface_vendor PUBLIC thrystr::glfw OpenGL::GL)
    thrystr_disable_vendor_warnings(thrystr_interface_vendor)
    add_library(thrystr::interface_vendor ALIAS thrystr_interface_vendor)

    add_library(thrystr_design_vendor STATIC
        src/gui/vendor/skald/src/theme.cpp
        src/gui/vendor/skald/src/widgets.cpp
        src/gui/vendor/skald/src/fonts.cpp)
    target_include_directories(thrystr_design_vendor PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gui/vendor/skald/include)
    target_link_libraries(thrystr_design_vendor PUBLIC thrystr::interface_vendor)
    thrystr_disable_vendor_warnings(thrystr_design_vendor)
    add_library(thrystr::design_vendor ALIAS thrystr_design_vendor)

    add_library(thrystr_image_vendor STATIC
        src/gui/vendor/stb/stb_image_impl.cpp
        src/gui/vendor/stb/stb_image_write_impl.cpp)
    target_include_directories(thrystr_image_vendor PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gui/vendor/stb)
    thrystr_disable_vendor_warnings(thrystr_image_vendor)
    add_library(thrystr::image_vendor ALIAS thrystr_image_vendor)

    add_library(thrystr_gui STATIC
        src/gui/system/component.cpp
        src/gui/system/image_pixels.cpp
        src/gui/system/image_png.cpp
        src/gui/system/image_texture.cpp
        src/gui/system/interface_session_frame.cpp
        src/gui/system/interface_session_lifecycle.cpp
        src/gui/system/interface_types.cpp
        src/gui/system/native_application.cpp
        src/gui/system/native_window_create.cpp
        src/gui/system/native_window_decoration.cpp
        src/gui/system/native_window_lifecycle.cpp
        src/gui/system/native_window_metrics.cpp
        src/gui/system/native_window_move.cpp
        src/gui/system/native_window_placement.cpp
        src/gui/system/native_window_x11.cpp
        src/gui/system/native_window_x11_move.cpp
        src/gui/system/native_window_x11_resize.cpp
        src/gui/system/native_work_area.cpp
        src/gui/system/resize_cursor_set.cpp
        src/gui/system/texture_upload.cpp)
    target_include_directories(thrystr_gui PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)
    target_link_libraries(thrystr_gui
        PUBLIC thrystr::design_vendor
        PRIVATE thrystr::image_vendor thrystr::glfw OpenGL::GL)
    target_compile_options(thrystr_gui PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>)
    if(UNIX AND NOT APPLE AND X11_FOUND)
        target_include_directories(thrystr_gui PRIVATE ${X11_INCLUDE_DIR})
        target_link_libraries(thrystr_gui PRIVATE ${X11_LIBRARIES})
        target_compile_definitions(thrystr_gui PRIVATE THRYSTR_HAS_X11=1)
    endif()

    add_executable(thrystr
        src/main.cpp)
    target_link_libraries(thrystr PRIVATE
        thrystr_core
        thrystr_app
        thrystr_gui)
    if(UNIX AND NOT APPLE AND X11_FOUND)
        target_include_directories(thrystr PRIVATE ${X11_INCLUDE_DIR})
        target_link_libraries(thrystr PRIVATE ${X11_LIBRARIES})
        target_compile_definitions(thrystr PRIVATE THRYSTR_HAS_X11=1)
    endif()
    target_compile_definitions(thrystr PRIVATE
        THRYSTR_SKALD_FONT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts"
        THRYSTR_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")
    target_compile_options(thrystr PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>)
endif()

if(THRYSTR_BUILD_DOCS)
    add_executable(thrystr_docgen
        tools/docgen/main.cpp)
    target_compile_features(thrystr_docgen PRIVATE cxx_std_20)
    target_compile_options(thrystr_docgen PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>)

    set(THRYSTR_GENERATED_DOCS_DIR "${CMAKE_BINARY_DIR}/generated/docs")
    set(THRYSTR_DOCS_SITE_DIR "${CMAKE_BINARY_DIR}/docs/site")
    set(THRYSTR_DOCS_CPP "${THRYSTR_GENERATED_DOCS_DIR}/docs_resources.cpp")
    set(THRYSTR_DOCS_HPP "${THRYSTR_GENERATED_DOCS_DIR}/docs_resources.hpp")
    file(GLOB THRYSTR_USER_DOCS CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/user/*.md")
    add_custom_command(
        OUTPUT "${THRYSTR_DOCS_CPP}" "${THRYSTR_DOCS_HPP}"
        COMMAND thrystr_docgen
            --input "${CMAKE_CURRENT_SOURCE_DIR}/docs/user"
            --site "${THRYSTR_DOCS_SITE_DIR}"
            --cpp "${THRYSTR_DOCS_CPP}"
            --hpp "${THRYSTR_DOCS_HPP}"
        DEPENDS thrystr_docgen ${THRYSTR_USER_DOCS}
        COMMENT "Generating thrystr manual resources"
        VERBATIM)
    add_custom_target(thrystr_docs ALL
        DEPENDS "${THRYSTR_DOCS_CPP}" "${THRYSTR_DOCS_HPP}")
    add_library(thrystr_docs_resources STATIC
        "${THRYSTR_DOCS_CPP}")
    target_include_directories(thrystr_docs_resources PUBLIC
        "${THRYSTR_GENERATED_DOCS_DIR}")
    add_dependencies(thrystr_docs_resources thrystr_docs)
    if(TARGET thrystr)
        add_dependencies(thrystr thrystr_docs)
        target_link_libraries(thrystr PRIVATE thrystr_docs_resources)
        target_compile_definitions(thrystr PRIVATE THRYSTR_HAS_DOCS=1)
    endif()
endif()

if(THRYSTR_BUILD_TESTS)
    enable_testing()
    add_executable(thrystr_core_tests
        tests/scalar_analysis_tests.cpp)
    target_link_libraries(thrystr_core_tests PRIVATE thrystr_core)
    add_test(NAME thrystr_core_tests COMMAND thrystr_core_tests)

    add_executable(thrystr_app_tests
        tests/app_model_tests.cpp)
    target_link_libraries(thrystr_app_tests PRIVATE thrystr_app)
    add_test(NAME thrystr_app_tests COMMAND thrystr_app_tests)

    add_executable(thrystr_model_tests
        tests/model_tests.cpp)
    target_link_libraries(thrystr_model_tests PRIVATE thrystr_model thrystr_ui)
    add_test(NAME thrystr_model_tests COMMAND thrystr_model_tests)

    add_executable(thrystr_multi_track_fit_tests
        tests/app_multi_track_fit_tests.cpp)
    target_link_libraries(thrystr_multi_track_fit_tests PRIVATE thrystr_app)
    add_test(NAME thrystr_multi_track_fit_tests COMMAND thrystr_multi_track_fit_tests)

    if(TARGET thrystr_gui)
        add_executable(thrystr_gui_tests
            tests/gui_boundary_tests.cpp)
        target_link_libraries(thrystr_gui_tests PRIVATE thrystr_gui)
        add_test(NAME thrystr_gui_tests COMMAND thrystr_gui_tests)

        add_executable(thrystr_gui_window_tests
            tests/gui_window_tests.cpp)
        target_link_libraries(thrystr_gui_window_tests PRIVATE thrystr_gui)
        add_test(NAME thrystr_gui_window_tests COMMAND thrystr_gui_window_tests)
    endif()
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
