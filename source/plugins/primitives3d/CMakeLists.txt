project(prim3d)

include_directories(
            ${PROJECT_SOURCE_DIR}
            ${MINDTREE_SRC_LIB_DIR}
)

link_directories(
            ${PROJECT_SOURCE_DIR}
            ${PROJECT_BINARY_DIR}/source/lib
            ${MINDTREE_CORE_LIB_DIR}
            ${MINDTREE_SRC_LIB_DIR}
            ${MAIN_INCLUDE_DIR}
)

set(PRIMITIVES3D_SRC
    cube.cpp
    cylinder.cpp
    plane.cpp
    sphere.cpp
    torus.cpp
    entry.cpp
    )

add_library(prim3d SHARED ${PRIMITIVES3D_SRC})
set_target_properties(prim3d PROPERTIES PREFIX "")

target_link_libraries(prim3d
                    mindtree_core
                    objectlib
                    ${MINDTREE_CORE_LIB}
)


install(TARGETS prim3d LIBRARY DESTINATION ${PROJECT_ROOT}/plugins/primitives3d/)


