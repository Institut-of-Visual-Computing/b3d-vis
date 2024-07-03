#include "glad/glad.h"

#include "NanoViewer.h"


#include <cuda.h>

#include "passes/DebugDrawPass.h"
#include "passes/FullscreenTexturePass.h"
#include "passes/InfinitGridPass.h"

#include <Logging.h>

#include <format>
#include <string>

#include "GLUtils.h"
#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_transform.hpp"

#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "owl/helper/cuda.h"

#include <ImGuizmo.h>

#include <cuda_runtime.h>
#include <filesystem>

#include <nvml.h>

#include "stb_image.h"
#include "tracy/Tracy.hpp"

using namespace owl;
using namespace owl::viewer;

namespace
{
	auto currentGizmoOperation(ImGuizmo::ROTATE);
	auto currentGizmoMode(ImGuizmo::LOCAL);

	auto reshape(GLFWwindow* window, const int width, const int height) -> void
	{
		auto gw = static_cast<OWLViewer*>(glfwGetWindowUserPointer(window));
		assert(gw);
		gw->resize(vec2i(width, height));
	}

	auto keyboardKey(GLFWwindow* window, const unsigned int key) -> void
	{
		auto& io = ImGui::GetIO();
		if (!io.WantCaptureKeyboard)
		{
			auto gw = static_cast<OWLViewer*>(glfwGetWindowUserPointer(window));
			assert(gw);
			gw->key(key, gw->getMousePos());
		}
	}

	auto keyboardSpecialKey(GLFWwindow* window, const int key, [[maybe_unused]] int scancode, const int action,
		const int mods) -> void
	{
		const auto gw = static_cast<OWLViewer*>(glfwGetWindowUserPointer(window));
		assert(gw);
		if (action == GLFW_PRESS)
		{
			gw->special(key, mods, gw->getMousePos());
		}
	}

	auto mouseMotion(GLFWwindow* window, const double x, const double y) -> void
	{
		const auto& io = ImGui::GetIO();
		if (!io.WantCaptureMouse)
		{
			const auto gw = static_cast<OWLViewer*>(glfwGetWindowUserPointer(window));
			assert(gw);
			gw->mouseMotion(vec2i(static_cast<int>(x), static_cast<int>(y)));
		}
	}

	auto mouseButton(GLFWwindow* window, const int button, const int action, const int mods) -> void
	{
		const auto& io = ImGui::GetIO();
		if (!io.WantCaptureMouse)
		{
			const auto gw = static_cast<OWLViewer*>(glfwGetWindowUserPointer(window));
			assert(gw);
			gw->mouseButton(button, action, mods);
		}
		else
		{
			//TODO: It's a hack!! We can complete decouple our viewer from OWLViewer by implementing our own camera.
			const auto gw = static_cast<OWLViewer*>(glfwGetWindowUserPointer(window));
			assert(gw);
			gw->mouseButton(button, GLFW_RELEASE, mods);
		}
	}

	std::vector<ImFont*> defaultFonts;
	std::unordered_map<float, int> scaleToFont{};
	int currentFontIndex{ 0 };

	auto rebuildFont() -> void
	{
		auto& io = ImGui::GetIO();

		io.Fonts->ClearFonts();
		defaultFonts.clear();

		constexpr auto baseFontSize = 16.0f;

		ImFontConfig config;

		config.OversampleH = 8;
		config.OversampleV = 8;

		auto monitorCount = 0;
		const auto monitors = glfwGetMonitors(&monitorCount);
		assert(monitorCount > 0);
		for (auto i = 0; i < monitorCount; i++)
		{
			const auto monitor = monitors[i];
			auto scaleX = 0.0f;
			auto scaleY = 0.0f;
			glfwGetMonitorContentScale(monitor, &scaleX, &scaleY);
			const auto dpiScale = scaleX; // / 96;

			config.SizePixels = dpiScale * baseFontSize;

			if (!scaleToFont.contains(scaleX))
			{
				auto font =
					io.Fonts->AddFontFromFileTTF("resources/fonts/Roboto-Medium.ttf", dpiScale * baseFontSize, &config);
				defaultFonts.push_back(font);
				scaleToFont[scaleX] = i;
			}
		}
	}

	auto windowContentScaleCallback([[maybe_unused]] GLFWwindow* window, const float scaleX,
		[[maybe_unused]] float scaleY)
	{
		if (!scaleToFont.contains(scaleX))
		{
			rebuildFont();
		}

		currentFontIndex = scaleToFont[scaleX];
		const auto dpiScale = scaleX; // / 96;
		ImGui::GetStyle().ScaleAllSizes(dpiScale);
	}


	auto initializeGui(GLFWwindow* window) -> void
	{
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuizmo::AllowAxisFlip(false);
		auto& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad; // Enable Gamepad Controls

		ImGui::StyleColorsDark();

		rebuildFont();

		ImGui_ImplGlfw_InitForOpenGL(window, true);
		ImGui_ImplOpenGL3_Init();
	}

	auto deinitializeGui() -> void
	{
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
	}

	std::unique_ptr<FullscreenTexturePass> fsPass;
	std::unique_ptr<InfinitGridPass> igPass;
	std::unique_ptr<DebugDrawPass> ddPass;

	std::unique_ptr<DebugDrawList> ddList;

	struct ViewerSettings
	{
		float lineWidth{ 4.0 };
		std::array<float, 3> gridColor{ 0.95f, 0.9f, 0.92f };
		bool enableDebugDraw{ true };
		bool enableGridFloor{ true };
	};

	ViewerSettings viewerSettings{};

} // namespace

auto NanoViewer::gui() -> void
{
	static auto showDemoWindow = true;
	ImGui::ShowDemoWindow(&showDemoWindow);

	currentRenderer_->gui();
	static auto showViewerSettings = true;
	ImGui::Begin("Viewer Settings", &showViewerSettings, ImGuiWindowFlags_AlwaysAutoResize);

	const auto& preview = registeredRendererNames_[selectedRendererIndex_];

	if (ImGui::BeginCombo("Renderer", preview.c_str()))
	{
		for (auto n = 0; n < registeredRendererNames_.size(); n++)
		{
			const auto isSelected = (selectedRendererIndex_ == n);
			if (ImGui::Selectable(registeredRendererNames_[n].c_str(), isSelected))
			{
				newSelectedRendererIndex_ = n;
			}

			if (isSelected)
			{
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}

	static auto selectedCameraControlIndex = 0;
	static constexpr auto controls = std::array{ "POI", "Fly" };

	if (ImGui::BeginCombo("Camera Control", controls[selectedCameraControlIndex]))
	{
		for (auto i = 0; i < controls.size(); i++)
		{
			const auto isSelected = i == selectedCameraControlIndex;
			if (ImGui::Selectable(controls[i], isSelected))
			{
				selectedCameraControlIndex = i;
				if (i == 0)
				{
					enableInspectMode();
				}
				if (i == 1)
				{
					enableFlyMode();
				}
			}
			if (isSelected)
			{
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}
	ImGui::Separator();

	ImGui::Separator();


	ImGui::Checkbox("Enable Grid Floor", &viewerSettings.enableGridFloor);

	if (viewerSettings.enableGridFloor)
	{
		ImGui::SeparatorText("Grid Settings");
		ImGui::ColorEdit3("Color", viewerSettings.gridColor.data());
		ImGui::Separator();
	}

	ImGui::Checkbox("Enable Debug Draw", &viewerSettings.enableDebugDraw);

	if (viewerSettings.enableDebugDraw)
	{
		ImGui::SeparatorText("Debug Draw Settings");
		ImGui::SliderFloat("Line Width", &viewerSettings.lineWidth, 1.0f, 10.0f);
		ImGui::Separator();


		if (ImGui::IsKeyPressed(ImGuiKey_1))
		{
			currentGizmoOperation = ImGuizmo::TRANSLATE;
		}
		if (ImGui::IsKeyPressed(ImGuiKey_2))
		{
			currentGizmoOperation = ImGuizmo::ROTATE;
		}
		if (ImGui::IsKeyPressed(ImGuiKey_3))
		{
			currentGizmoOperation = ImGuizmo::SCALE;
		}
	}

	ImGui::SeparatorText("NVML Settings");


	static auto enablePersistenceMode{ false };
	static auto enabledPersistenceMode{ false };
	static auto showPermissionDeniedMessage{ false };

	uint32_t clock;
	{
		const auto error = nvmlDeviceGetClockInfo(nvmlDevice_, NVML_CLOCK_SM, &clock);
		assert(error == NVML_SUCCESS);
	}

	ImGui::BeginDisabled(!isAdmin_);
	ImGui::Checkbox(std::format("Max GPU SM Clock [current: {} MHz]", clock).c_str(), &enablePersistenceMode);
	ImGui::EndDisabled();
	if (enablePersistenceMode != enabledPersistenceMode)
	{
		if (enablePersistenceMode)
		{
			const auto error =
				nvmlDeviceSetGpuLockedClocks(nvmlDevice_, static_cast<unsigned int>(NVML_CLOCK_LIMIT_ID_TDP),
					static_cast<unsigned int>(NVML_CLOCK_LIMIT_ID_TDP));

			enabledPersistenceMode = true;

			assert(error == NVML_SUCCESS);
		}

		else
		{
			const auto error = nvmlDeviceResetGpuLockedClocks(nvmlDevice_);

			enabledPersistenceMode = false;
			enablePersistenceMode = false;
			assert(error == NVML_SUCCESS);
		}
	}


	if (!isAdmin_)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{ 0.9f, 0.1f, 0.1f, 1.0f });
		ImGui::TextWrapped("This Application should run in admin mode to apply the effect of this option!");
		ImGui::PopStyleColor();
		ImGui::AlignTextToFramePadding();
	}
	// const auto rr = nvmlDeviceSetPersistenceMode(nvmlDevice, enablePersistenceMode?NVML_FEATURE_ENABLED:
	// NVML_FEATURE_DISABLED);


	ImGui::End();
}

auto NanoViewer::render() -> void
{
	constexpr auto layout = static_cast<GLuint>(GL_LAYOUT_GENERAL_EXT);

	const auto cam = b3d::renderer::Camera{ .origin = camera.getFrom(),
											.at = camera.getAt(),
											.up = camera.getUp(),
											.cosFoV = camera.getCosFovy(),
											.FoV = glm::radians(camera.fovyInDegrees) };

	renderingData_.data.view = b3d::renderer::View{
		.cameras = { cam, cam },
		.mode = b3d::renderer::RenderMode::mono,
	};

	renderingData_.data.renderTargets = b3d::renderer::RenderTargets{
		.colorRt = { cuDisplayTexture, { static_cast<uint32_t>(fbSize.x), static_cast<uint32_t>(fbSize.y), 1 } },
		.minMaxRt = { cuDisplayTexture, { static_cast<uint32_t>(fbSize.x), static_cast<uint32_t>(fbSize.y), 1 } },
	};

	// GL_CALL(glSignalSemaphoreEXT(synchronizationResources_.glSignalSemaphore, 0, nullptr, 0, nullptr, &layout));

	currentRenderer_->render();

	// NOTE: this function call return error, when the semaphore wasn't used before (or it could be in the wrong initial
	// state)
	// GL_CALL(glWaitSemaphoreEXT(synchronizationResources_.glWaitSemaphore, 0, nullptr, 0, nullptr, nullptr));
}
auto NanoViewer::resize(const owl::vec2i& newSize) -> void
{
	OWLViewer::resize(newSize);
	cameraChanged();
}

auto NanoViewer::cameraChanged() -> void
{
	OWLViewer::cameraChanged();
}

auto NanoViewer::onFrameBegin() -> void
{
	if (newSelectedRendererIndex_ != selectedRendererIndex_)
	{
		selectRenderer(newSelectedRendererIndex_);
	}
}

NanoViewer::NanoViewer(const std::string& title, const int initWindowWidth, const int initWindowHeight,
	bool enableVsync, const int rendererIndex)
	: owl::viewer::OWLViewer(title, owl::vec2i(initWindowWidth, initWindowHeight), true, enableVsync), resources_{},
	renderingData_{}, synchronizationResources_{}, colorMapResources_{}
					   bool enableVsync, const int rendererIndex)
	: resources_{}, renderingData_{}, colorMapResources_{}
{
	nvmlInit();


	{
		const auto error =
			nvmlDeviceGetHandleByIndex(renderingData_.data.rendererInitializationInfo.deviceIndex, &nvmlDevice_);
		assert(error == NVML_SUCCESS);
	}

	{
		const auto error = nvmlDeviceResetGpuLockedClocks(nvmlDevice_);
		if (error == NVML_ERROR_NO_PERMISSION)
		{
			isAdmin_ = false;
		}
		if (error == NVML_SUCCESS)
		{
			isAdmin_ = true;
		}
		assert(error == NVML_SUCCESS || error == NVML_ERROR_NO_PERMISSION);
	}


	gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress));
	gladLoadGL();


	// Create Colormap and load data from default colormap, if present
	{
		GL_CALL(glGenTextures(1, &colorMapResources_.colormapTexture));
		GL_CALL(glBindTexture(GL_TEXTURE_2D, colorMapResources_.colormapTexture));

		// Setup filtering parameters for display
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // This is required on WebGL for non power-of-two textures
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); // Same

		// Load default colormap
		auto filePath = std::filesystem::path{ "resources/colormaps" };
		if (std::filesystem::exists(filePath / "defaultColorMap.json"))
		{
			colorMapResources_.colorMap = b3d::tools::colormap::load(filePath / "defaultColorMap.json");

			if (std::filesystem::path(colorMapResources_.colorMap.colorMapFilePath).is_relative())
			{
				filePath /= colorMapResources_.colorMap.colorMapFilePath;
			}
			else
			{
				filePath = colorMapResources_.colorMap.colorMapFilePath;
			}
			int x, y, n;

			const auto bla = stbi_info(filePath.string().c_str(), &x, &y, &n);

			auto data = stbi_loadf(filePath.string().c_str(), &x, &y, &n, 0);

			// Load Colormap
			GL_CALL(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, x, y, 0, GL_RGBA, GL_FLOAT, data));

			stbi_image_free(data);

			renderingData_.data.colorMapTexture.extent =
				b3d::renderer::Extent{ static_cast<uint32_t>(x), static_cast<uint32_t>(y), 1 };
			renderingData_.data.colorMapTexture.nativeHandle = reinterpret_cast<void*>(colorMapResources_.colormapTexture);
		}
		else
		{
			GL_CALL(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 512, 1, 0, GL_RGBA, GL_FLOAT, nullptr));
			renderingData_.data.colorMapTexture.extent =
				b3d::renderer::Extent{ 512, 1, 1 };
		}

		std::string colormaptexturename = "ColorMap";
		GL_CALL(glObjectLabel(GL_TEXTURE, colorMapResources_.colormapTexture, colormaptexturename.length() + 1,
			colormaptexturename.c_str()));

		// TODO: add cuda error checks
		auto rc =
			cudaGraphicsGLRegisterImage(&colorMapResources_.cudaGraphicsResource, colorMapResources_.colormapTexture, GL_TEXTURE_2D, cudaGraphicsRegisterFlagsTextureGather);

		renderingData_.data.colorMapTexture.target = colorMapResources_.cudaGraphicsResource;

		renderingData_.data.coloringInfo =
			b3d::renderer::ColoringInfo{ b3d::renderer::ColoringMode::single, vec4f{ 1, 1, 1, 1 },
										 colorMapResources_.colorMap.firstColorMapYTextureCoordinate };

		renderingData_.data.colorMapInfos =
			b3d::renderer::ColorMapInfos{ &colorMapResources_.colorMap.colorMapNames,
										  colorMapResources_.colorMap.colorMapCount,
										  colorMapResources_.colorMap.firstColorMapYTextureCoordinate,
										  colorMapResources_.colorMap.colorMapHeightNormalized };


	}

	// Transferfunction
	{
		GL_CALL(glGenTextures(1, &transferFunctionResources_.transferFunctionTexture));
		GL_CALL(glBindTexture(GL_TEXTURE_2D, transferFunctionResources_.transferFunctionTexture));

		// Setup filtering parameters for display
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // This is required on WebGL for non power-of-two textures
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); // Same



		std::array<float, 512> initBufferData;

		std::ranges::fill(initBufferData, 1);
		GL_CALL(glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, 512, 1, 0, GL_RED, GL_FLOAT, initBufferData.data()));

		std::string transferFunctionBufferName = "TransferFunctionTexture";
		GL_CALL(glObjectLabel(GL_TEXTURE, transferFunctionResources_.transferFunctionTexture,
			transferFunctionBufferName.length() + 1, transferFunctionBufferName.c_str()));

		cudaError rc = cudaGraphicsGLRegisterImage(&transferFunctionResources_.cudaGraphicsResource,
			transferFunctionResources_.transferFunctionTexture, GL_TEXTURE_2D, cudaGraphicsRegisterFlagsTextureGather | cudaGraphicsRegisterFlagsWriteDiscard);

		renderingData_.data.transferFunctionTexture.extent = { 512, 1, 1 };
		renderingData_.data.transferFunctionTexture.target = transferFunctionResources_.cudaGraphicsResource;
		renderingData_.data.transferFunctionTexture.nativeHandle =
			reinterpret_cast<void*>(transferFunctionResources_.transferFunctionTexture);
	}

	// NOTE: rendererInfo will be fed into renderer initialization

	selectRenderer(rendererIndex);
	newSelectedRendererIndex_ = selectedRendererIndex_;

	for (auto i = 0; i < b3d::renderer::registry.size(); i++)
	{
		registeredRendererNames_.push_back(b3d::renderer::registry[i].name);
	}
}

auto NanoViewer::showAndRunWithGui() -> void
{
	showAndRunWithGui([]() { return true; });
}

auto NanoViewer::drawGizmos(const CameraMatrices& cameraMatrices) -> void
{
	ImGui::Begin("##GizmoOverlay", nullptr,
		ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus |
		ImGuiWindowFlags_NoNavFocus);
	{
		ImGui::SetWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y), 0);
		ImGui::SetWindowPos(ImVec2(0, 0));
		ImGuizmo::SetDrawlist();
		ImGuizmo::SetRect(0, 0, ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y);
		for (const auto transform : gizmoHelper_->getTransforms())
		{
			float mat[16];

			mat[3] = 0.0f;
			mat[7] = 0.0f;
			mat[11] = 0.0f;

			mat[12] = transform->p.x;
			mat[13] = transform->p.y;
			mat[14] = transform->p.z;

			mat[15] = 1.0f;

			mat[0] = transform->l.vx.x;
			mat[1] = transform->l.vx.y;
			mat[2] = transform->l.vx.z;

			mat[4] = transform->l.vy.x;
			mat[5] = transform->l.vy.y;
			mat[6] = transform->l.vy.z;

			mat[8] = transform->l.vz.x;
			mat[9] = transform->l.vz.y;
			mat[10] = transform->l.vz.z;
			ImGuizmo::Manipulate(reinterpret_cast<const float*>(&cameraMatrices.view),
				reinterpret_cast<const float*>(&cameraMatrices.projection), currentGizmoOperation,
				currentGizmoMode, mat, nullptr, nullptr);

			transform->p.x = mat[12];
			transform->p.y = mat[13];
			transform->p.z = mat[14];

			transform->l.vx = owl::vec3f{ mat[0], mat[1], mat[2] };
			transform->l.vy = owl::vec3f{ mat[4], mat[5], mat[6] };
			transform->l.vz = owl::vec3f{ mat[8], mat[9], mat[10] };
		}
		auto blockInput = false;

		for (const auto& [bound, transform, worldTransform] : gizmoHelper_->getBoundTransforms())
		{
			float mat[16];

			mat[3] = 0.0f;
			mat[7] = 0.0f;
			mat[11] = 0.0f;

			mat[12] = transform->p.x;
			mat[13] = transform->p.y;
			mat[14] = transform->p.z;

			mat[15] = 1.0f;

			mat[0] = transform->l.vx.x;
			mat[1] = transform->l.vx.y;
			mat[2] = transform->l.vx.z;

			mat[4] = transform->l.vy.x;
			mat[5] = transform->l.vy.y;
			mat[6] = transform->l.vy.z;

			mat[8] = transform->l.vz.x;
			mat[9] = transform->l.vz.y;
			mat[10] = transform->l.vz.z;


			const auto halfSize = bound / 2.0f;

			const auto bounds = std::array{ halfSize.x, halfSize.y, halfSize.z, -halfSize.x,-halfSize.y,-halfSize.z };

			glm::mat4 worldTransformMat{ { worldTransform.l.vx.x, worldTransform.l.vx.y, worldTransform.l.vx.z, 0.0f },
										 { worldTransform.l.vy.x, worldTransform.l.vy.y, worldTransform.l.vy.z, 0.0f },
										 { worldTransform.l.vz.x, worldTransform.l.vz.y, worldTransform.l.vz.z, 0.0f },
										 { worldTransform.p.x, worldTransform.p.y, worldTransform.p.z, 1.0f } };
			const auto matX = cameraMatrices.view * worldTransformMat;
			
			ImGuizmo::Manipulate(reinterpret_cast<const float*>(&matX),
				reinterpret_cast<const float*>(&cameraMatrices.projection), ImGuizmo::OPERATION::BOUNDS,
				currentGizmoMode, mat, nullptr, nullptr, bounds.data());
			if (ImGuizmo::IsUsing())
			{
				blockInput = true;
			}

			transform->p.x = mat[12];
			transform->p.y = mat[13];
			transform->p.z = mat[14];

			transform->l.vx = owl::vec3f{ mat[0], mat[1], mat[2] };
			transform->l.vy = owl::vec3f{ mat[4], mat[5], mat[6] };
			transform->l.vz = owl::vec3f{ mat[8], mat[9], mat[10] };
		}

		if (blockInput)
		{
#if IMGUI_VERSION_NUM >= 18723
			ImGui::SetNextFrameWantCaptureMouse(true);
#else
			ImGui::CaptureMouseFromApp();
#endif
		}
	}
	ImGui::End();
}
auto NanoViewer::computeViewProjectionMatrixFromCamera(const owl::viewer::Camera& camera, const int width,
	const int height) -> CameraMatrices
{
	const auto aspect = width / static_cast<float>(height);

	CameraMatrices mat;
	mat.projection = glm::perspective(glm::radians(camera.getFovyInDegrees()), aspect, 0.01f, 10000.0f);
	mat.view = glm::lookAt(glm::vec3{ camera.position.x, camera.position.y, camera.position.z },
		glm::vec3{ camera.getAt().x, camera.getAt().y, camera.getAt().z },
		glm::normalize(glm::vec3{ camera.getUp().x, camera.getUp().y, camera.getUp().z }));


	mat.viewProjection = mat.projection * mat.view;
	return mat;
}

auto NanoViewer::showAndRunWithGui(const std::function<bool()>& keepgoing) -> void
{
	gladLoadGL();

	ddList = std::make_unique<DebugDrawList>();
	fsPass = std::make_unique<FullscreenTexturePass>();
	igPass = std::make_unique<InfinitGridPass>();
	ddPass = std::make_unique<DebugDrawPass>(debugDrawList_.get());

	int width, height;
	glfwGetFramebufferSize(handle, &width, &height);
	resize(vec2i(width, height));

	glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
	glfwSetFramebufferSizeCallback(handle, reshape);
	glfwSetMouseButtonCallback(handle, ::mouseButton);
	glfwSetKeyCallback(handle, keyboardSpecialKey);
	glfwSetCharCallback(handle, keyboardKey);
	glfwSetCursorPosCallback(handle, ::mouseMotion);
	glfwSetWindowContentScaleCallback(handle, windowContentScaleCallback);

	initializeGui(handle);
	glfwMakeContextCurrent(handle);

	while (!glfwWindowShouldClose(handle) && keepgoing())
	{
		{
			ZoneScoped;

			//TODO: if windows minimized or not visible -> skip rendering
			onFrameBegin();
			glClear(GL_COLOR_BUFFER_BIT);
			static double lastCameraUpdate = -1.f;
			if (camera.lastModified != lastCameraUpdate)
			{
				cameraChanged();
				lastCameraUpdate = camera.lastModified;
			}
			gizmoHelper_->clear();

			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplGlfw_NewFrame();
			ImGui::NewFrame();
			ImGui::PushFont(defaultFonts[currentFontIndex]);
			gui();

			const auto cameraMatrices = computeViewProjectionMatrixFromCamera(camera, fbSize.x, fbSize.y);

			if (viewerSettings.enableDebugDraw)
			{
				drawGizmos(cameraMatrices);
			}

			ImGui::PopFont();
			ImGui::EndFrame();

			render();

			fsPass->setViewport(fbSize.x, fbSize.y);
			fsPass->setSourceTexture(fbTexture);
			fsPass->execute();


			if (viewerSettings.enableGridFloor)
			{
				igPass->setViewProjectionMatrix(cameraMatrices.viewProjection);
				igPass->setViewport(fbSize.x, fbSize.y);
				igPass->setGridColor(
					glm::vec3{ viewerSettings.gridColor[0], viewerSettings.gridColor[1], viewerSettings.gridColor[2] });
				igPass->execute();
			}

			if (viewerSettings.enableDebugDraw)
			{
				ddPass->setViewProjectionMatrix(cameraMatrices.viewProjection);
				ddPass->setViewport(fbSize.x, fbSize.y);
				ddPass->setLineWidth(viewerSettings.lineWidth);
				ddPass->execute();
			}

			ImGui::Render();
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

			glfwSwapBuffers(handle);
			glfwPollEvents();
			FrameMark;
		}
	}

	deinitializeGui();
	currentRenderer_->deinitialize();
	glfwDestroyWindow(handle);
	glfwTerminate();
}
NanoViewer::~NanoViewer()
{
	cudaGraphicsUnregisterResource(transferFunctionResources_.cudaGraphicsResource);
	cudaGraphicsUnregisterResource(colorMapResources_.cudaGraphicsResource);

	if (isAdmin_)
	{
		const auto error = nvmlDeviceResetGpuLockedClocks(nvmlDevice_);
		assert(error == NVML_SUCCESS);
	}
	nvmlShutdown();
}
auto NanoViewer::selectRenderer(const std::uint32_t index) -> void
{
	assert(index < b3d::renderer::registry.size());
	if (selectedRendererIndex_ == index)
	{
		return;
	}
	if (currentRenderer_)
	{
		currentRenderer_->deinitialize();
	}


	selectedRendererIndex_ = index;
	currentRenderer_ = b3d::renderer::registry[selectedRendererIndex_].rendererInstance;

	const auto debugInfo = b3d::renderer::DebugInitializationInfo{ debugDrawList_, gizmoHelper_ };

	currentRenderer_->initialize(&renderingData_.buffer, debugInfo);
}
