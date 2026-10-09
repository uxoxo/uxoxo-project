#*******************************************************************************
# uxoxo [build]                                                   CMakeLists.txt
#
# uxoxo's own build: the library, the Dear ImGui it carries, and two ImGui
# backends, as targets a consumer links by name.
#
#     uxoxo::imgui               Dear ImGui (inc/imgui) and its std::string
#                                InputText helpers, compiled once
#     uxoxo::uxoxo               the element core and the ImGui platform;
#                                links uxoxo::imgui and djinterp's headers
#     uxoxo::imgui_glfw_opengl3  the GLFW + OpenGL 3 backend, where GLFW and
#                                OpenGL are found
#     uxoxo::imgui_win32_dx11    the Win32 + Direct3D 11 backend, on Windows
#
#   It lives under build/cmake, where djinterp keeps its CMake, so a
# consumer's search for it (vparse's cmake/Uxoxo.cmake looks at the root,
# then build/cmake) finds it there. As a subproject it takes djinterp from
# the parent's djinterp::headers target; on its own it needs DJINTERP_ROOT.
#   LEVELS. uxoxo's headers reach djinterp's free monad and option record,
# whose floor is C++17, so UXOXO_CXX_STANDARD is 17, 20 or 23. Nothing else
# is imposed: no warning set, and no Windows macro.
#
# path:      /build/cmake/CMakeLists.txt
# link(s):   TBA
# author(s): Samuel 'teer' Neal-Blim                         created: 2026.10.04
#                                                            revised: 2026.10.04
#*******************************************************************************
cmake_minimum_required(VERSION 3.21)

project(uxoxo
    VERSION     0.2.0
    DESCRIPTION "Declarative UI over djinterp, with a Dear ImGui backend"
    LANGUAGES   CXX)

get_filename_component(UXOXO_SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.."
                       ABSOLUTE)

set(UXOXO_CXX_STANDARD 20 CACHE STRING "C++ level for uxoxo: 17, 20 or 23")
set_property(CACHE UXOXO_CXX_STANDARD PROPERTY STRINGS 17 20 23)

if(NOT UXOXO_CXX_STANDARD MATCHES "^(17|20|23)$")
    message(FATAL_ERROR
        "UXOXO_CXX_STANDARD is '${UXOXO_CXX_STANDARD}'; it must be 17, 20 or "
        "23. uxoxo's headers include djinterp headers whose floor is C++17.")
endif()

option(UXOXO_IMGUI_DEMO   "Compile imgui_demo.cpp into uxoxo::imgui" ON)
option(UXOXO_BUILD_TESTS  "Build uxoxo's own tests"  ${PROJECT_IS_TOP_LEVEL})


#-------------------------------------------------------------------------------
# djinterp's headers
#-------------------------------------------------------------------------------
if(TARGET djinterp::headers)
    set(_uxoxo_djinterp djinterp::headers)
else()
    set(DJINTERP_ROOT "" CACHE PATH
        "djinterp's repository root: the directory with inc/ and src/")

    if(NOT EXISTS "${DJINTERP_ROOT}/inc/djinterp/djinterp.hpp")
        message(FATAL_ERROR
            "uxoxo needs djinterp's headers. Set -DDJINTERP_ROOT=<path> to "
            "djinterp's repository root, or add uxoxo from a project that "
            "defines djinterp::headers.")
    endif()

    add_library(uxoxo_djinterp_headers INTERFACE)
    target_include_directories(uxoxo_djinterp_headers SYSTEM INTERFACE
        "${DJINTERP_ROOT}/inc")
    set(_uxoxo_djinterp uxoxo_djinterp_headers)
endif()


# uxoxo_set_level(<target>)
#   The level and the source character set, and nothing else.
function(uxoxo_set_level _target)
    set_target_properties(${_target} PROPERTIES
        CXX_STANDARD              ${UXOXO_CXX_STANDARD}
        CXX_STANDARD_REQUIRED     ON
        CXX_EXTENSIONS            OFF
        POSITION_INDEPENDENT_CODE ON
        FOLDER                    "uxoxo")

    # the trees are UTF-8 with no byte-order mark; cl must be told
    if(MSVC)
        target_compile_options(${_target} PRIVATE /utf-8)
    endif()
endfunction()


#-------------------------------------------------------------------------------
# Dear ImGui
#-------------------------------------------------------------------------------
set(_imgui "${UXOXO_SOURCE_ROOT}/inc/imgui")

if(NOT EXISTS "${_imgui}/imgui.cpp")
    message(FATAL_ERROR "uxoxo: no Dear ImGui at ${_imgui}")
endif()

set(_imgui_sources
    "${_imgui}/imgui.cpp"
    "${_imgui}/imgui_draw.cpp"
    "${_imgui}/imgui_tables.cpp"
    "${_imgui}/imgui_widgets.cpp"
    "${_imgui}/misc/cpp/imgui_stdlib.cpp")

if(UXOXO_IMGUI_DEMO)
    list(APPEND _imgui_sources "${_imgui}/imgui_demo.cpp")
endif()

add_library(uxoxo_imgui STATIC ${_imgui_sources})
add_library(uxoxo::imgui ALIAS uxoxo_imgui)

# SYSTEM: ImGui is not this project's code, and a consumer's warnings are
# not its business
target_include_directories(uxoxo_imgui SYSTEM PUBLIC "${_imgui}")
uxoxo_set_level(uxoxo_imgui)


#-------------------------------------------------------------------------------
# uxoxo
#-------------------------------------------------------------------------------
add_library(uxoxo STATIC
    "${UXOXO_SOURCE_ROOT}/src/uxoxo/platform/imgui_platform.cpp")
add_library(uxoxo::uxoxo ALIAS uxoxo)

target_include_directories(uxoxo PUBLIC "${UXOXO_SOURCE_ROOT}/inc")
target_link_libraries(uxoxo PUBLIC uxoxo::imgui ${_uxoxo_djinterp})
uxoxo_set_level(uxoxo)


#-------------------------------------------------------------------------------
# backends
#-------------------------------------------------------------------------------
# GLFW + OpenGL 3: wherever GLFW is found. ImGui's OpenGL loader opens the
# library itself, so the link needs dlopen and nothing from OpenGL directly.
find_package(glfw3 3.3 QUIET)

if(NOT TARGET glfw)
    find_package(PkgConfig QUIET)

    if(PkgConfig_FOUND)
        pkg_check_modules(UXOXO_GLFW3 QUIET IMPORTED_TARGET glfw3)
    endif()
endif()

if(TARGET glfw)
    set(_uxoxo_glfw glfw)
elseif(TARGET PkgConfig::UXOXO_GLFW3)
    set(_uxoxo_glfw PkgConfig::UXOXO_GLFW3)
endif()

if(_uxoxo_glfw)
    add_library(uxoxo_imgui_glfw_opengl3 STATIC
        "${_imgui}/backends/imgui_impl_glfw.cpp"
        "${_imgui}/backends/imgui_impl_opengl3.cpp")
    add_library(uxoxo::imgui_glfw_opengl3 ALIAS uxoxo_imgui_glfw_opengl3)

    target_include_directories(uxoxo_imgui_glfw_opengl3 SYSTEM PUBLIC
        "${_imgui}/backends")
    target_link_libraries(uxoxo_imgui_glfw_opengl3 PUBLIC
        uxoxo::imgui ${_uxoxo_glfw} ${CMAKE_DL_LIBS})

    if(APPLE)
        target_link_libraries(uxoxo_imgui_glfw_opengl3 PUBLIC
            "-framework OpenGL")
    endif()

    uxoxo_set_level(uxoxo_imgui_glfw_opengl3)
    message(STATUS "uxoxo: GLFW + OpenGL 3 backend available")
else()
    message(STATUS "uxoxo: GLFW not found; no GLFW + OpenGL 3 backend")
endif()

# Win32 + Direct3D 11: on Windows, with nothing to find
if(WIN32)
    add_library(uxoxo_imgui_win32_dx11 STATIC
        "${_imgui}/backends/imgui_impl_win32.cpp"
        "${_imgui}/backends/imgui_impl_dx11.cpp")
    add_library(uxoxo::imgui_win32_dx11 ALIAS uxoxo_imgui_win32_dx11)

    target_include_directories(uxoxo_imgui_win32_dx11 SYSTEM PUBLIC
        "${_imgui}/backends")

    # cl takes these from the backends' #pragma comment; MinGW must be told
    target_link_libraries(uxoxo_imgui_win32_dx11 PUBLIC
        uxoxo::imgui d3d11 d3dcompiler dxgi dwmapi gdi32)

    uxoxo_set_level(uxoxo_imgui_win32_dx11)
    message(STATUS "uxoxo: Win32 + Direct3D 11 backend available")
endif()


#-------------------------------------------------------------------------------
# tests
#-------------------------------------------------------------------------------
if(UXOXO_BUILD_TESTS)
    enable_testing()

    # The smoke test compiles its own ImGui, with every ID clash recorded
    # (IMGUI_DEBUG_HIGHLIGHT_ALL_ID_CONFLICTS): without it ImGui notices a
    # clash only under the mouse. A second copy is why it does not link
    # uxoxo::uxoxo, and why the define never reaches a consumer.
    add_executable(uxoxo_platform_smoke
        "${UXOXO_SOURCE_ROOT}/tests/platform/imgui_platform_smoke.cpp"
        "${UXOXO_SOURCE_ROOT}/src/uxoxo/platform/imgui_platform.cpp"
        "${_imgui}/imgui.cpp"
        "${_imgui}/imgui_draw.cpp"
        "${_imgui}/imgui_tables.cpp"
        "${_imgui}/imgui_widgets.cpp"
        "${_imgui}/misc/cpp/imgui_stdlib.cpp"
        "${_imgui}/backends/imgui_impl_null.cpp")
    target_include_directories(uxoxo_platform_smoke PRIVATE
        "${UXOXO_SOURCE_ROOT}/inc")
    target_include_directories(uxoxo_platform_smoke SYSTEM PRIVATE
        "${_imgui}"
        "${_imgui}/backends")
    target_compile_definitions(uxoxo_platform_smoke PRIVATE
        IMGUI_DEBUG_HIGHLIGHT_ALL_ID_CONFLICTS)
    target_link_libraries(uxoxo_platform_smoke PRIVATE ${_uxoxo_djinterp})
    uxoxo_set_level(uxoxo_platform_smoke)

    add_test(NAME uxoxo_platform_smoke COMMAND uxoxo_platform_smoke)
    set_tests_properties(uxoxo_platform_smoke PROPERTIES
        PASS_REGULAR_EXPRESSION "passed: 9   failed: 0")
endif()
