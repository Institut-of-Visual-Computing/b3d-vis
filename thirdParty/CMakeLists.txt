find_package(glfw3 CONFIG REQUIRED)
set(owl_dir ${CMAKE_CURRENT_LIST_DIR}/owl)
add_subdirectory(${owl_dir} EXCLUDE_FROM_ALL)
add_subdirectory(${owl_dir}/samples/common)
# add_subdirectory(${owl_dir}/samples/common)

add_subdirectory(imGuizmo)
add_subdirectory(imCurve)
add_subdirectory(imSpinner)

set_property(TARGET owl PROPERTY FOLDER "ThirdParty/owl")
set_property(TARGET owl_viewer PROPERTY FOLDER "ThirdParty/owl")

add_library(ImGuiUtils INTERFACE)

target_include_directories(ImGuiUtils
    INTERFACE
        "ImGuiUtils"
)   

add_library(B3D::ImGuiUtils ALIAS ImGuiUtils)


add_subdirectory(iconFontCppHeaders)
