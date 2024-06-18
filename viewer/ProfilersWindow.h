#pragma once

#include <chrono>
#include <string>
#include <sstream>

#include "ImGuiProfilerRenderer.h"

class ProfilersWindow
{
public:
	ProfilersWindow() : cpuGraph(300), gpuGraph(300)
	{
		stopProfiling = false;
		frameOffset = 0;
		frameWidth = 3;
		frameSpacing = 1;
		useColoredLegendText = true;
		prevFpsFrameTime = std::chrono::system_clock::now();
		fpsFramesCount = 0;
		avgFrameTime = 1.0f;
	}
	void Render()
	{
		fpsFramesCount++;
		auto currFrameTime = std::chrono::system_clock::now();
		{
			float fpsDeltaTime = std::chrono::duration<float>(currFrameTime - prevFpsFrameTime).count();
			if (fpsDeltaTime > 0.5f)
			{
				this->avgFrameTime = fpsDeltaTime / float(fpsFramesCount);
				fpsFramesCount = 0;
				prevFpsFrameTime = currFrameTime;
			}
		}

		std::stringstream title;
		title.precision(2);
		title << std::fixed << "Legit profiler [" << 1.0f / avgFrameTime << "fps\t" << avgFrameTime * 1000.0f
			  << "ms]###ProfilerWindow";
		// ###AnimatedTitle
		ImGui::Begin(title.str().c_str(), 0, ImGuiWindowFlags_NoScrollbar);
		ImVec2 canvasSize = ImGui::GetContentRegionAvail();

		int sizeMargin = int(ImGui::GetStyle().ItemSpacing.y);
		int maxGraphHeight = 200;
		int availableGraphHeight = (int(canvasSize.y) - sizeMargin); // / 2;
		int graphHeight = std::min(maxGraphHeight, availableGraphHeight);
		int legendWidth = 400;
		int graphWidth = int(canvasSize.x) - legendWidth;
		gpuGraph.RenderTimings(graphWidth, legendWidth, graphHeight, frameOffset);

		if (!stopProfiling)
			frameOffset = 0;
		gpuGraph.frameWidth = frameWidth;
		gpuGraph.frameSpacing = frameSpacing;
		gpuGraph.useColoredLegendText = useColoredLegendText;
		cpuGraph.frameWidth = frameWidth;
		cpuGraph.frameSpacing = frameSpacing;
		cpuGraph.useColoredLegendText = useColoredLegendText;

		ImGui::End();
	}
	bool stopProfiling;
	int frameOffset;
	ImGuiUtils::ProfilerGraph cpuGraph;
	ImGuiUtils::ProfilerGraph gpuGraph;
	int frameWidth;
	int frameSpacing;
	bool useColoredLegendText;
	using TimePoint = std::chrono::time_point<std::chrono::system_clock>;
	TimePoint prevFpsFrameTime;
	size_t fpsFramesCount;
	float avgFrameTime;
};
