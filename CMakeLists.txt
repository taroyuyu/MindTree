cmake_minimum_required(VERSION 3.5)
set(CMAKE_CXX_STANDARD 14)
project(mindTree)

option(CLANG_SANITIZE_THREAD "CLANG_SANITIZE_THREAD" OFF)
option(CLANG_SANITIZE_ADDRESS "CLANG_SANITIZE_ADDRESS" OFF)
option(PROFILE "PROFILE" OFF)

SET(HEADER
#            source/data/code_generator/shaderwriter.h
#            source/data/code_generator/rslwriter.h
#            source/data/code_generator/glslwriter.h
#            source/data/code_generator/inputs.h
#            source/data/code_generator/outputs.h
            source/graphics/python/consolewindow.h
            source/graphics/base/mindtree_mainwindow.h
)

SET(SOURCE
#            source/data/code_generator/shaderwriter.cpp
#            source/data/code_generator/rslwriter.cpp
#            source/data/code_generator/glslwriter.cpp
#            source/data/code_generator/inputs.cpp
#            source/data/code_generator/outputs.cpp
            source/data/base/init.cpp
            source/data/gl/init.cpp
            source/data/python/init.cpp
            #source/graphics/gui.cpp
            source/graphics/python/consolewindow.cpp
            source/data/base/main.cpp
            source/graphics/base/consoledialog.cpp
            source/data/undo/frg_generic_undo.cpp
            source/graphics/base/mindtree_mainwindow.cpp
)

set(RESOURCES source/graphics/styles/styles.qrc)

#enable C++11 features
execute_process(COMMAND ${CMAKE_C_COMPILER} -dumpversion
                OUTPUT_VARIABLE GCC_VERSION)
message("compiler flags: " ${CMAKE_CXX_FLAGS})

message("C++ compiler: " ${CMAKE_CXX_COMPILER_ID})
message("CMake Build Type: " ${CMAKE_BUILD_TYPE})

set(CMAKE_EXE_LINKER_FLAGS "-lX11 ${CMAKE_EXE_LINKER_FLAGS}")

#set(CMAKE_VERBOSE_MAKEFILE true)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions("-fno-omit-frame-pointer")
    add_definitions("-O0")
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        if(CLANG_SANITIZE_THREAD)
            set(CMAKE_EXE_LINKER_FLAGS "-fsanitize=thread ${CMAKE_EXE_LINKER_FLAGS}")
            set(CMAKE_SHARED_LINKER_FLAGS "-fsanitize=thread ${CMAKE_SHARED_LINKER_FLAGS}")
            message("compiling with clang thread sanitizer")
        elseif(CLANG_SANITIZE_ADDRESS)
            add_definitions("-fsanitize=address")
            set(CMAKE_EXE_LINKER_FLAGS "-fsanitize=address ${CMAKE_EXE_LINKER_FLAGS}")
            set(CMAKE_SHARED_LINKER_FLAGS "-fsanitize=address ${CMAKE_SHARED_LINKER_FLAGS}")
            message("compiling with clang address sanitizer")
        else()
            message("no or wrong sanitize option specified")
        endif()
        if(PROFILE)
            set(PROFILE_LIB profiler)
        endif()
    endif()
else()
    if(CLANG_SANITIZE_THREAD)
        add_definitions("-fsanitize=thread")
        add_definitions("-fno-omit-frame-pointer")
        set(CMAKE_EXE_LINKER_FLAGS "-fsanitize=thread ${CMAKE_EXE_LINKER_FLAGS}")
        set(CMAKE_SHARED_LINKER_FLAGS "-fsanitize=thread ${CMAKE_SHARED_LINKER_FLAGS}")
        message("compiling with clang thread sanitizer")
    endif()
    add_definitions("-O3")
    add_definitions("-msse2")
endif()
    
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
ADD_DEFINITIONS(-DQT_NO_KEYWORDS)
find_package(Qt5Widgets REQUIRED)
find_package(Qt5Core REQUIRED)
find_package(Qt5OpenGL REQUIRED)
find_package(PythonLibs 3.4 REQUIRED)
find_package(Boost COMPONENTS python REQUIRED)
# set(Boost_LIBRARIES /usr/lib/x86_64-linux-gnu/libboost_python-py34.so)
# set(PYTHON_LIBRARIES /usr/lib/python3.4/config-3.4m-x86_64-linux-gnu/libpython3.4.so)
# set(PYTHON_INCLUDE_DIRS /usr/include/python3.4)
find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)

set(MINDTREE_CORE_LIB_DIR ${PROJECT_BINARY_DIR}/lib)
set(MINDTREE_SRC_LIB_DIR ${PROJECT_SOURCE_DIR}/source/lib)
include_directories(
            ${MINDTREE_SRC_LIB_DIR}
            ${PROJECT_SOURCE_DIR}
            ${Boost_INCLUDE_DIRS}
            ${PYTHON_INCLUDE_DIRS}
            ${OPENGL_ICNLUDE_DIRS}
            ${GLEW_ICNLUDE_DIRS}
)

link_directories(
            ${PROJECT_SOURCE_DIR}
            ${MINDTREE_CORE_LIB_DIR}
            ${PROJECT_BINARY_DIR}/source/lib
            ${MINDTREE_SRC_LIB_DIR}
            ${Boost_LIBRARY_DIRS}
            ${PYTHON_LIBRARIES}
            ${OPENGL_LIBRARY_DIRS}
            ${GLEW_LIBRARY_DIRS}
)

set(ALL_SRC ${SOURCE} ${RESOURCES})
set(MINDTREE_CORE_LIB ${PROJECT_BINARY_DIR}/source/lib/libmindtree_core.so)

set(CMAKE_SKIP_BUILD_RPATH FALSE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
set(CMAKE_INSTALL_RPATH "../lib")

add_subdirectory(source/lib)

add_executable(mindTree ${ALL_SRC})
target_link_libraries(mindTree
  X11
  mindtree_core
  ${MINDTREE_CORE_LIB}
  Qt5::Widgets
  Qt5::Core
  Qt5::OpenGL
  ${Boost_LIBRARIES}
  ${PYTHON_LIBRARIES}
  ${OPENGL_LIBRARIES}
  ${GLEW_LIBRARIES}
  ${PROFILE_LIB}
)

set(PROJECT_ROOT ${PROJECT_BINARY_DIR})
add_subdirectory(source/intern)
add_subdirectory(source/plugins)
add_subdirectory(source/processors)


message("Binary Dir:" ${PROJECT_BINARY_DIR})
message("mindtree core lib:" ${MINDTREE_CORE_LIB})
message("Python Libs: " ${PYTHON_LIBRARIES})
install(TARGETS mindTree RUNTIME DESTINATION ${PROJECT_BINARY_DIR}/bin)
install(DIRECTORY source/plugins DESTINATION ${PROJECT_BINARY_DIR} PATTERN "*.h|*.cpp" EXCLUDE)
install(DIRECTORY source/intern DESTINATION ${PROJECT_BINARY_DIR} PATTERN "*.h|*.cpp" EXCLUDE)
install(DIRECTORY source/settings DESTINATION ${PROJECT_BINARY_DIR} PATTERN "*.h|*.cpp" EXCLUDE)

