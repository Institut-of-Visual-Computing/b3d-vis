set(MODULE_NAME "ActionNativeTexture")
add_library(${MODULE_NAME} SHARED
	"src/ActionNativeTexture.cpp"
)

target_link_libraries(${MODULE_NAME}
	PRIVATE
		B3D::UnityCUDAInterop::PluginInterface
		B3D::UnityCUDAInterop::ActionInterface
		B3D::Renderer::Library
		# Replace the next line with the renderer you would like to bring to Unity
		B3D::Renderer::TextureFillerSyncSample
		d3d11.lib
)

set_target_properties(${MODULE_NAME} PROPERTIES CXX_STANDARD 20)

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "UnityCudaInterop/Actions")

# Unity Samples directory
install(TARGETS ${MODULE_NAME} DESTINATION ${UNITY_PROJECT_ROOT_DIR}/Assets/${UNITY_ASSET_SUBDIRECTORY_INTEROP_ACTION_SAMPLES}/${MODULE_NAME})

# Unity Actions directory
# install(TARGETS ${MODULE_NAME} DESTINATION ${UNITY_PROJECT_ROOT_DIR}/Assets/${UNITY_ASSET_SUBDIRECTORY_INTEROP_ACTIONS}/${MODULE_NAME})
