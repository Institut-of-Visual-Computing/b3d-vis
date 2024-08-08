#pragma once
#include <ColorMap.h>
#include <RendererBase.h>

#include <nvml.h>


#include "Camera.h"
#include "DebugDrawList.h"
#include "GizmoHelper.h"


class NanoViewer final
{
public:
	explicit NanoViewer(const std::string& title = "Nano Viewer", int initWindowWidth = 1980,
						int initWindowHeight = 1080, bool enableVsync = false, int rendererIndex = 0);
	auto showAndRunWithGui() -> void;
	auto showAndRunWithGui(const std::function<bool()>& keepgoing) -> void;
	[[nodiscard]] auto getCamera() -> ::Camera&
	{
		return camera_;
	}
	~NanoViewer();

private:
	auto selectRenderer(uint32_t index) -> void;
	auto gui() -> void;
	auto draw() -> void;
	auto onFrameBegin() -> void;


	struct CameraMatrices
	{
		glm::mat4 view;
		glm::mat4 projection;
		glm::mat4 viewProjection;
	};

	std::shared_ptr<DebugDrawList> debugDrawList_{};
	std::shared_ptr<GizmoHelper> gizmoHelper_{};

	std::shared_ptr<b3d::renderer::RendererBase> currentRenderer_{ nullptr };
	std::int32_t selectedRendererIndex_{ -1 };
	std::int32_t newSelectedRendererIndex_{ -1 };
	std::vector<std::string> registeredRendererNames_{};

	nvmlDevice_t nvmlDevice_{};
	bool isAdmin_{ false };

	::Camera camera_{};

	bool isRunning_{ true };
};
