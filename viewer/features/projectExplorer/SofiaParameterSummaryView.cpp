#include "SofiaParameterSummaryView.h"
#include "StaticColors.h"

SofiaParameterSummaryView::SofiaParameterSummaryView(ApplicationContext& applicationContext)
	: ModalViewBase{ applicationContext, "Sofia Parameter Summary", ModalType::ok }
{
}
auto SofiaParameterSummaryView::onDraw() -> void
{
	const float max_height = ImGui::GetIO().DisplaySize.y / 2.0f;
	const float min_width = 400.0f;
	const float content_width = ImGui::GetContentRegionAvail().x;

	ImGui::BeginChild("ParameterScrollArea", ImVec2(min_width, max_height), true,
					  ImGuiWindowFlags_HorizontalScrollbar);

	unblock();

	ImGui::Separator();

	auto labelId = 0;
	// Helper-Lambda zur Parameter-Anzeige mit Fallback auf Default-Werte
	auto showParam = [&](const std::string& paramName)
	{
		ImGui::PushID(labelId);
		auto value = params_.getStringValue(paramName);

		const auto isDefaultValueAvailable = b3d::tools::sofia::DEFAULT_PARAMS.at(paramName) != "";
		const auto valueAvailable = value.has_value();
		ImGui::Text(paramName.c_str());

		if (isDefaultValueAvailable || valueAvailable)
		{
			ImGui::PushStyleColor(ImGuiCol_Text, gui::colors::ImColors::OK());
		}
		else
		{
			ImGui::PushStyleColor(ImGuiCol_Text, gui::colors::ImColors::WARNING());
		}

		auto valueText = value ? value->c_str() : b3d::tools::sofia::DEFAULT_PARAMS.at(paramName).c_str();
		if (!isDefaultValueAvailable)
		{
			valueText = "Undefined";
		}

		ImGui::SameLine(200 * ImGui::GetWindowDpiScale());
		ImGui::Text(valueText);
		ImGui::PopStyleColor();
		ImGui::PopID();
		labelId++;
	};

	// Global Settings
	if (ImGui::CollapsingHeader("Global Settings", ImGuiTreeNodeFlags_DefaultOpen))
	{
		showParam("pipeline.verbose");
		showParam("pipeline.pedantic");
		showParam("pipeline.threads");
	}

	// Input
	if (ImGui::CollapsingHeader("Input"))
	{
		showParam("input.data");
		showParam("input.region");
		showParam("input.gain");
		showParam("input.noise");
		showParam("input.weights");
		showParam("input.primaryBeam");
		showParam("input.mask");
		showParam("input.invert");
	}

	// Flagging
	if (ImGui::CollapsingHeader("Flagging"))
	{
		showParam("flag.region");
		showParam("flag.catalog");
		showParam("flag.radius");
		showParam("flag.auto");
		showParam("flag.threshold");
		showParam("flag.log");
	}

	// Continuum Subtraction
	if (ImGui::CollapsingHeader("Continuum Subtraction"))
	{
		showParam("contsub.enable");
		showParam("contsub.order");
		showParam("contsub.threshold");
		showParam("contsub.shift");
		showParam("contsub.padding");
	}

	// Noise Scaling
	if (ImGui::CollapsingHeader("Noise Scaling"))
	{
		showParam("scaleNoise.enable");
		showParam("scaleNoise.mode");
		showParam("scaleNoise.statistic");
		showParam("scaleNoise.fluxRange");
		showParam("scaleNoise.windowXY");
		showParam("scaleNoise.windowZ");
		showParam("scaleNoise.gridXY");
		showParam("scaleNoise.gridZ");
		showParam("scaleNoise.interpolate");
		showParam("scaleNoise.scfind");
	}

	// Ripple Filter
	if (ImGui::CollapsingHeader("Ripple Filter"))
	{
		showParam("rippleFilter.enable");
		showParam("rippleFilter.statistic");
		showParam("rippleFilter.windowXY");
		showParam("rippleFilter.windowZ");
		showParam("rippleFilter.gridXY");
		showParam("rippleFilter.gridZ");
		showParam("rippleFilter.interpolate");
	}

	// S+C Finder
	if (ImGui::CollapsingHeader("S+C Finder"))
	{
		showParam("scfind.enable");
		showParam("scfind.kernelsXY");
		showParam("scfind.kernelsZ");
		showParam("scfind.threshold");
		showParam("scfind.replacement");
		showParam("scfind.statistic");
		showParam("scfind.fluxRange");
	}

	// Threshold Finder
	if (ImGui::CollapsingHeader("Threshold Finder"))
	{
		showParam("threshold.enable");
		showParam("threshold.threshold");
		showParam("threshold.mode");
		showParam("threshold.statistic");
		showParam("threshold.fluxRange");
	}

	// Linker
	if (ImGui::CollapsingHeader("Linker"))
	{
		showParam("linker.enable");
		showParam("linker.radiusXY");
		showParam("linker.radiusZ");
		showParam("linker.minSizeXY");
		showParam("linker.minSizeZ");
		showParam("linker.maxSizeXY");
		showParam("linker.maxSizeZ");
		showParam("linker.minPixels");
		showParam("linker.maxPixels");
		showParam("linker.minFill");
		showParam("linker.maxFill");
		showParam("linker.positivity");
		showParam("linker.keepNegative");
	}

	// Reliability
	if (ImGui::CollapsingHeader("Reliability"))
	{
		showParam("reliability.enable");
		showParam("reliability.parameters");
		showParam("reliability.threshold");
		showParam("reliability.scaleKernel");
		showParam("reliability.minSNR");
		showParam("reliability.minPixels");
		showParam("reliability.autoKernel");
		showParam("reliability.iterations");
		showParam("reliability.tolerance");
		showParam("reliability.catalog");
		showParam("reliability.plot");
		showParam("reliability.debug");
	}

	// Mask Dilation
	if (ImGui::CollapsingHeader("Mask Dilation"))
	{
		showParam("dilation.enable");
		showParam("dilation.iterationsXY");
		showParam("dilation.iterationsZ");
		showParam("dilation.threshold");
	}

	// Parameterisation
	if (ImGui::CollapsingHeader("Parameterisation"))
	{
		showParam("parameter.enable");
		showParam("parameter.wcs");
		showParam("parameter.physical");
		showParam("parameter.prefix");
		showParam("parameter.offset");
	}

	// Output
	if (ImGui::CollapsingHeader("Output"))
	{
		showParam("output.directory");
		showParam("output.filename");
		showParam("output.writeCatASCII");
		showParam("output.writeCatXML");
		showParam("output.writeCatSQL");
		showParam("output.writeNoise");
		showParam("output.writeFiltered");
		showParam("output.writeMask");
		showParam("output.writeMask2d");
		showParam("output.writeRawMask");
		showParam("output.writeMoments");
		showParam("output.writeCubelets");
		showParam("output.writePV");
		showParam("output.writeKarma");
		showParam("output.marginCubelets");
		showParam("output.thresholdMom12");
		showParam("output.overwrite");
	}

	ImGui::Separator();

	// End of scrollable area
	ImGui::EndChild();
}
