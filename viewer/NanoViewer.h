#pragma once
#include <nvml.h>
#include <owlViewer/OWLViewer.h>
#include "DebugDrawList.h"

#include <RendererBase.h>

#include "ColorMap.h"
#include "GizmoHelper.h"

class NanoViewer final : public owl::viewer::OWLViewer
{
public:
	explicit NanoViewer(const std::string& title = "Sample Viewer", const int initWindowWidth = 1980,
						const int initWindowHeight = 1080, bool enableVsync = false, const int rendererIndex = 0);
	auto showAndRunWithGui() -> void;
	auto showAndRunWithGui(const std::function<bool()>& keepgoing) -> void;
	virtual ~NanoViewer();

private:
	auto selectRenderer(const std::uint32_t index) -> void;
	auto gui() -> void;
	auto render() -> void override;
	auto resize(const owl::vec2i& newSize) -> void override;
	auto cameraChanged() -> void override;
	auto onFrameBegin() -> void;


	struct CameraMatrices
	{
		glm::mat4 view;
		glm::mat4 projection;
		glm::mat4 viewProjection;
	};

	auto drawGizmos(const CameraMatrices& cameraMatrices) -> void;
	static auto computeViewProjectionMatrixFromCamera(const owl::viewer::Camera& camera, const int width,
													  const int height) -> CameraMatrices;


	std::shared_ptr<DebugDrawList> debugDrawList_{};
	std::shared_ptr<GizmoHelper> gizmoHelper_{};

	std::shared_ptr<b3d::renderer::RendererBase> currentRenderer_{ nullptr };
	std::int32_t selectedRendererIndex_{ -1 };
	std::int32_t newSelectedRendererIndex_{ -1 };
	std::vector<std::string> registeredRendererNames_{};

	struct GraphicsResources
	{
		GLuint colorTexture;
		GLuint minMaxTexture;

	} resources_;
	
	struct ColorMapResources
	{
		b3d::tools::colormap::ColorMap colorMap;
		GLuint colormapTexture;
		cudaGraphicsResource_t cudaGraphicsResource;
	} colorMapResources_;

	struct TransferFunctionResources
	{
		GLuint transferFunctionTexture;
		cudaGraphicsResource_t cudaGraphicsResource;
	} transferFunctionResources_;


	b3d::renderer::RenderingDataWrapper renderingData_;

	b3d::renderer::RenderMode mode_{ b3d::renderer::RenderMode::mono };

	nvmlDevice_t nvmlDevice_{};
	bool isAdmin_{ false };

};
