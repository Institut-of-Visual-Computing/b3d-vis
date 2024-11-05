#define IMGUI_DEFINE_MATH_OPERATORS
#include "TransferMappingView.h"


#include <Curve.h>

#include <imgui_internal.h>
#include "imgui.h"
#include "implot.h"

TransferMappingView::TransferMappingView(ApplicationContext& appContext, Dockspace* dockspace)
	: DockableWindowViewBase{ appContext, "Transfer Mapping", dockspace, WindowFlagBits::none }
{
	dataPoints_.resize(10);
	dataPoints_[0] = { 0.00000000f, 0.00000000f };
	dataPoints_[1] = { 0.00158982514f, 0.420000017f };
	dataPoints_[2] = { 0.0810810775f, 0.779999971f };
	dataPoints_[3] = { 0.270270258f, 0.845000029f };
	dataPoints_[4] = { 0.511923671f, 1.00000000f };
	dataPoints_[5] = { 0.686804473f, 0.654999971f };
	dataPoints_[6] = { 0.812400639f, 0.754999995f };
	dataPoints_[7] = { 1.00000000f, 0.605000019f };
	dataPoints_[8].x = ImGui::CurveTerminator;
}

auto TransferMappingView::resampleData(const int samplesCount) const -> std::vector<float>
{
	auto samples = std::vector<float>{};
	samples.resize(samplesCount);

	const auto inc = 1.0f / (samples.size() - 1);
	for (auto i = 0; i < samples.size(); i++)
	{
		samples[i] = ImGui::CurveValue(i * inc, dataPoints_.size(), dataPoints_.data());
	}
	return samples;
}

auto TransferMappingView::onDraw() -> void
{
	const auto availableSize = ImGui::GetContentRegionAvail();
	const auto size = ImVec2{ availableSize.x, std::min({ 200.0f, availableSize.y }) };
	newDataAvailable_ = false;

	{
		const auto totalItems = colorMapNames_.size();
		assert(colorMapTextureHandle_);

		ImGui::Combo("Mode", &selectedColoringMode_, "Uniform Color\0ColorMap\0\0");
		if (selectedColoringMode_ == 0)
		{
			if (ImGui::ColorEdit3("Uniform Color", &uniformColor_.x))
			{
				newDataAvailable_ = true;
			}
		}
		else
		{
			ImGui::SetNextItemWidth(-1);
			if (ImGui::BeginCombo("##coloringModeSelector", "", ImGuiComboFlags_CustomPreview))
			{
				const auto mapItemSize = ImVec2{ ImGui::GetContentRegionAvail().x, 20 };
				ImGui::Image(colorMapTextureHandle_, mapItemSize,
							 ImVec2(0, (selectedColoringMap_ + 0.5) / static_cast<float>(totalItems)),
							 ImVec2(1, (selectedColoringMap_ + 0.5) / static_cast<float>(totalItems)));

				for (auto n = 0; n < totalItems; n++)
				{
					const auto isSelected = (selectedColoringMap_ == n);
					if (ImGui::Selectable(std::format("##colorMap{}", n).c_str(), isSelected,
										  ImGuiSelectableFlags_AllowOverlap, mapItemSize))
					{
						newDataAvailable_ = true;
						selectedColoringMap_ = n;
					}
					ImGui::SameLine(1);
					ImGui::Image(colorMapTextureHandle_, mapItemSize,
								 ImVec2(0, (n + 0.5) / static_cast<float>(totalItems)),
								 ImVec2(1, (n + 0.5) / static_cast<float>(totalItems)));

					if (isSelected)
					{
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}

			if (ImGui::BeginComboPreview())
			{
				const auto mapItemSize = ImVec2{ ImGui::GetContentRegionAvail().x, 20 };
				ImGui::Image(colorMapTextureHandle_, mapItemSize,
							 ImVec2(0, selectedColoringMap_ / static_cast<float>(totalItems)),
							 ImVec2(1, (selectedColoringMap_ + 1) / static_cast<float>(totalItems)));
				ImGui::EndComboPreview();
			}
		}
	}

	static auto bar_data = std::array{ 100, 80, 20, 34, 26, 53, 436, 13, 47, 94 };
	/*float x_data[1000] = ...;
	float y_data[1000] = ...;*/

	struct PointHandle
	{
		ImPlotPoint point;
		ImVec2 controlOffset; // those offsets are symmetrical to point
	};
	static auto selectedPointIndex = -1;
	static auto pointHandles =
		std::vector<PointHandle>{ { ImPlotPoint{ 0, 0 }, ImVec2{ 1, 0 } }, { ImPlotPoint{ 1, 80 }, ImVec2{ 1, 0 } } };
	if (ImPlot::BeginPlot("My Plot", ImVec2(-1, 0), ImPlotFlags_NoLegend | ImPlotFlags_NoMenus))
	{
		ImPlot::SetupAxis(ImAxis_X1, "intensity", ImPlotAxisFlags_RangeFit);
		ImPlot::SetupAxis(ImAxis_Y1, "num", ImPlotAxisFlags_RangeFit);
		ImPlot::SetupFinish();

		ImPlot::PlotBars("My Bar Plot", bar_data.data(), bar_data.size());

		auto anyPointIsDragging = false;

		static auto evaluatedCurveValues = std::array<ImPlotPoint, 100>{};
		const auto segmentsPerSpline = evaluatedCurveValues.size() / (pointHandles.size() - 1);

		for (auto splineSegment = 0; splineSegment < pointHandles.size() - 1; splineSegment++)
		{
			for (auto i = 0; i < segmentsPerSpline; i++)
			{
				const auto t = i / (double)segmentsPerSpline;
				const auto u = 1 - t;
				const auto w1 = u * u * u;
				const auto w2 = 3 * u * u * t;
				const auto w3 = 3 * u * t * t;
				const auto w4 = t * t * t;
				evaluatedCurveValues[splineSegment * segmentsPerSpline + i] = ImPlotPoint(
					w1 * pointHandles[splineSegment].point.x +
						w2 * (pointHandles[splineSegment].point.x + pointHandles[splineSegment].controlOffset.x) +
						w3 *
							(pointHandles[splineSegment + 1].point.x -
							 pointHandles[splineSegment + 1].controlOffset.x) +
						w4 * pointHandles[splineSegment + 1].point.x,
					w1 * pointHandles[splineSegment].point.y +
						w2 * (pointHandles[splineSegment].point.y + pointHandles[splineSegment].controlOffset.y) +
						w3 *
							(pointHandles[splineSegment + 1].point.y -
							 pointHandles[splineSegment + 1].controlOffset.y) +
						w4 * pointHandles[splineSegment + 1].point.y);
			}
		}

		ImPlot::PlotLine("##bez", &evaluatedCurveValues[0].x, &evaluatedCurveValues[0].y, evaluatedCurveValues.size(),
						 0, 0, sizeof(ImPlotPoint));

		for (auto i = 0; i < pointHandles.size(); i++)
		{
			const auto isFirst = i == 0;
			const auto isLast = i == pointHandles.size() - 1;
			auto& pointHandle = pointHandles[i];
			const auto isClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);

			const auto isDragging = ImPlot::DragPoint(
				i, &pointHandle.point.x, &pointHandle.point.y,
				selectedPointIndex == i ? ImVec4{ 0, 1, 0, 1 } : ImVec4{ 1, 0, 0, 1 }, 5, ImPlotDragToolFlags_None);
			if (isFirst)
			{
				// apply position constrains
				pointHandle.point.y = 0;
			}
			anyPointIsDragging |= isDragging;
			if (isDragging or isClicked)
			{
				selectedPointIndex = i;
			}

			if (selectedPointIndex == i)
			{
				ImGui::PushID("handler_control");
				// draw handles


				auto lineSegment = std::array<ImPlotPoint, 2>{};
				if (isFirst)
				{
					// apply position constrains
					pointHandle.point.y = 0;
					lineSegment[0] = pointHandle.point;
					lineSegment[1] = ImPlotPoint{ pointHandle.point.x + pointHandle.controlOffset.x,
												  pointHandle.point.y + pointHandle.controlOffset.y };
				}
				else
				{
					lineSegment[0] = ImPlotPoint{ pointHandle.point.x - pointHandle.controlOffset.x,
												  pointHandle.point.y - pointHandle.controlOffset.y };
					lineSegment[1] = ImPlotPoint{ pointHandle.point.x + pointHandle.controlOffset.x,
												  pointHandle.point.y + pointHandle.controlOffset.y };


					const auto isLeftDrag =
						ImPlot::DragPoint(i, &lineSegment[0].x, &lineSegment[0].y, ImVec4{ 0, 0, 1, 1 }, 5,
										  ImPlotDragToolFlags_None | ImPlotDragToolFlags_Delayed);

					if (isLeftDrag)
					{
						if (lineSegment[0].x > pointHandle.point.x)
						{
							lineSegment[0].x = pointHandle.point.x;
						}
						pointHandle.controlOffset = ImVec2{ (float)(pointHandle.point.x - lineSegment[0].x),
															(float)(pointHandle.point.y - lineSegment[0].y) };
					}
				}
				ImPlot::PlotLine("##h1", &lineSegment[0].x, &lineSegment[0].y, 2, 0, 0, sizeof(ImPlotPoint));
				ImGui::PopID();
			}
		}

		/*if (not anyPointIsDragging)
		{
			selectedPointIndex = -1;
		}*/
		if (ImPlot::IsPlotHovered())
		{
			/*ImPlot::PushPlotClipRect();
			auto& DrawList = *ImPlot::GetPlotDrawList();

			const auto pos = ImPlot::PlotToPixels(*x, *y, IMPLOT_AUTO, IMPLOT_AUTO);
			DrawList.AddCircleFilled(pos, radius, col32);
			ImPlot::PopPlotClipRect();*/
		}
		if (ImPlot::IsPlotHovered() and ImGui::IsMouseClicked(ImGuiMouseButton_Left))
		{
			const auto newPoint = ImPlot::GetPlotMousePos();
			pointHandles.push_back(PointHandle{ newPoint, ImVec2{ 1, 0 } });
		}

		// ImPlot::PlotLine("My Line Plot", x_data, y_data, 1000);
		ImPlot::EndPlot();
	}

	// TODO:: Curve crashes sometimes in release
	if (ImGui::Curve("##transferFunction", size, dataPoints_.size(), dataPoints_.data(), &selectedCurveHandleIdx_))
	{
		newDataAvailable_ = true;
	}
}
