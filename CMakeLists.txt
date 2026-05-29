#---------------------------------------------------------------------------------------------------
# CMake Build Configuration for the Ray Tracing Weekend Series
#
# See README.md for guidance.
#---------------------------------------------------------------------------------------------------

cmake_minimum_required ( VERSION 3.1.0...4.0.0 )

project ( RTWeekend LANGUAGES CXX )

set ( CMAKE_CXX_STANDARD          17 )
set ( CMAKE_CXX_STANDARD_REQUIRED ON )
set ( CMAKE_CXX_EXTENSIONS        OFF )

# Source

set ( main
  src/main.cc
)

include_directories(inc)

# Specific compiler flags below. We're not going to add options for all possible compilers, but if
# you're new to CMake (like we are), the following may be a helpful example if you're using a
# different compiler or want to set different compiler options.

message (STATUS "Compiler ID: " ${CMAKE_CXX_COMPILER_ID})
message (STATUS "Release flags: " ${CMAKE_CXX_FLAGS_RELEASE})
message (STATUS "Debug flags: " ${CMAKE_CXX_FLAGS_DEBUG})

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    # /wd #### - Disable warning
    # /we #### - treat warning as error
    add_compile_options("/W4")      # Enable level-4 warnings
    add_compile_options("/we 4265") # Class has virtual functions, but its non-trivial destructor is not virtual
    add_compile_options("/we 5204") # Class has virtual functions, but its trivial destructor is not virtual
    add_compile_options("/wd 4100") # unreferenced formal parameter
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-Wnon-virtual-dtor) # Class has virtual functions, but its destructor is not virtual
    add_compile_options(-Wreorder) # Data member will be initialized after [other] data member
    add_compile_options(-Wmaybe-uninitialized) # Variable improperly initialized
    add_compile_options(-Wunused-variable) # Variable is defined but unused
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-Wnon-virtual-dtor) # Class has virtual functions, but its destructor is not virtual
    add_compile_options(-Wreorder) # Data member will be initialized after [other] data member
    add_compile_options(-Wsometimes-uninitialized) # Variable improperly initialized
    add_compile_options(-Wunused-variable) # Variable is defined but unused
endif()

# Executables
add_executable(main     ${main})

# GPU renderer (Phase 2). Only built where a CUDA toolkit is present so the CPU
# baseline still configures on machines without nvcc.
include(CheckLanguage)
check_language(CUDA)
if (CMAKE_CUDA_COMPILER)
    enable_language(CUDA)
    set(CMAKE_CUDA_STANDARD          17)
    set(CMAKE_CUDA_STANDARD_REQUIRED ON)
    add_executable(main_cuda cuda/render.cu)
    target_include_directories(main_cuda PRIVATE cuda)
    set_target_properties(main_cuda PROPERTIES CUDA_ARCHITECTURES "80")  # A30
    message(STATUS "CUDA found - building main_cuda")

    # Interactive viewer (Phase 2): orbit the model in real time, GPU re-renders
    # each frame and blits to an X11 window. Needs X11; skip if headers absent.
    find_package(X11)
    if (X11_FOUND)
        add_executable(main_viewer cuda/viewer.cu)
        target_include_directories(main_viewer PRIVATE cuda ${X11_INCLUDE_DIR})
        target_link_libraries(main_viewer PRIVATE ${X11_LIBRARIES})
        set_target_properties(main_viewer PROPERTIES CUDA_ARCHITECTURES "80")
        message(STATUS "X11 found - building main_viewer (interactive)")
    else()
        message(STATUS "X11 not found - skipping main_viewer")
    endif()

    # RT Studio (Phase 2): full Qt5 GUI with a real-time GPU-ray-traced viewport.
    # Qt and CUDA stay in separate translation units, bridged by gui/renderer.h.
    find_package(Qt5 COMPONENTS Widgets QUIET)
    if (Qt5_FOUND)
        set(CMAKE_AUTOMOC ON)
        add_executable(main_gui
            gui/main_gui.cpp
            gui/mainwindow.cpp
            gui/renderview.cpp
            gui/renderworker.cpp
            gui/panels.cpp
            gui/scene.cpp
            gui/renderer.cu)
        target_include_directories(main_gui PRIVATE cuda gui)
        target_link_libraries(main_gui PRIVATE Qt5::Widgets)
        set_target_properties(main_gui PROPERTIES
            CUDA_ARCHITECTURES "80"
            AUTOMOC ON)
        message(STATUS "Qt5 found - building main_gui (RT Studio)")
    else()
        message(STATUS "Qt5 not found - skipping main_gui")
    endif()
else()
    message(STATUS "CUDA not found - skipping main_cuda (GPU renderer)")
endif()