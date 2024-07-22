#pragma once
#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include "Camera.h"
#include "framework/DockableWindowViewBase.h"
#include "GLUtils.h"
#include "GizmoOperationFlags.h"

#include "imgui.h"

#include <array>
#include <cuda_runtime.h>
#include <memory>


class GizmoHelper;

class FullscreenTexturePass;
class InfinitGridPass;
class DebugDrawPass;

class DebugDrawList;

namespace b3d::renderer
{
	class RendererBase;
	class RenderingDataWrapper;
} // namespace b3d::renderer

class VolumeView final : public DockableWindowViewBase
{
public:
	VolumeView(ApplicationContext& appContext, Dockspace* dockspace);
	~VolumeView();

	auto onDraw() -> void override;
	auto onResize() -> void override;

	struct CameraMatrices
	{
		glm::mat4 view;
		glm::mat4 projection;
		glm::mat4 viewProjection;
	};


	auto setRenderVolume(b3d::renderer::RendererBase* renderer, b3d::renderer::RenderingDataWrapper* renderingData)
		-> void;

private:
	auto drawGizmos(const CameraMatrices& cameraMatrices, const glm::vec2& position, const glm::vec2& size) -> void;
	auto initializeGraphicsResources() -> void;
	auto deinitializeGraphicsResources() -> void;

	auto renderVolume() -> void;

	b3d::renderer::RendererBase* renderer_{};
	b3d::renderer::RenderingDataWrapper* renderingData_{};

	Camera camera_{};

	struct ViewerSettings
	{
		float lineWidth{ 4.0 };
		std::array<float, 3> gridColor{ 0.95f, 0.9f, 0.92f };
		bool enableDebugDraw{ true };
		bool enableGridFloor{ true };
	};

	ViewerSettings viewerSettings_{};

	struct GraphicsResources
	{
		GLuint framebuffer;
		GLuint framebufferTexture{ 0 };


		cudaGraphicsResource_t cuFramebufferTexture{ 0 };
		uint32_t* framebufferPointer{ nullptr };
		glm::vec2 framebufferSize{ 0 };
	};


	GraphicsResources graphicsResources_{};

	GizmoOperationFlags currentGizmoOperation_{ GizmoOperationFlagBits::none };

	std::unique_ptr<FullscreenTexturePass> fullscreenTexturePass_;
	std::unique_ptr<InfinitGridPass> InfinitGridPass_{};
	std::unique_ptr<DebugDrawPass> debugDrawPass_{};
};