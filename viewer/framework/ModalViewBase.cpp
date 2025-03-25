#include "ModalViewBase.h"

#include "IdGenerator.h"

#include <format>
#include <features/SoFiASubregionFeature.h>
#include <SofiaParams.h>
#include <features/projectExplorer/SofiaParameterSummaryView.h>

ModalViewBase::ModalViewBase(ApplicationContext& applicationContext, const std::string_view name,
							 const ModalType modalType, const ImVec2& minSize)
	: applicationContext_{ &applicationContext }, modalType_{ modalType },
	  id_{ std::format("{}###modal{}", name, IdGenerator::next()) }, minSize_{ minSize }
{
}

auto ModalViewBase::open() -> void
{
	isOpenRequested_ = true;
	if (onOpenCallback_)
	{
		onOpenCallback_(this);
	}
}

auto ModalViewBase::draw() -> void
{
	if (isOpenRequested_)
	{
		ImGui::OpenPopup(id_.c_str(), ImGuiPopupFlags_AnyPopup);
		isOpenRequested_ = false;
	}
	const auto center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSizeConstraints(ImVec2(300, 300), ImVec2{ 600, 600 });
	// ImGui::OpenPopup(id_.c_str());
	if (ImGui::BeginPopupModal(id_.c_str(), nullptr, ImGuiWindowFlags_NoResize))
	{
		onDraw();

		// Überschrift und Separator
		ImGui::Text("Sofia Parameters");
		ImGui::Separator();

		// Helper-Lambda zur Parameter-Anzeige mit Fallback auf Default-Werte
		auto showParam = [this](const std::string& paramName)
		{
			auto value = sofiaParams_.getStringValue(paramName);
			ImGui::Text("%s: %s", paramName.c_str(),
						value ? value->c_str() : b3d::tools::sofia::DEFAULT_PARAMS.at(paramName).c_str());
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

		// [Weitere Sektionen...]

		ImGui::Separator();

		// Buttons
		if (ImGui::Button("OK", ImVec2(120, 0)))
		{
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120, 0)))
		{
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
}

auto ModalViewBase::reset() -> void
{
	block();
}

auto ModalViewBase::submit() -> void
{
	if (!isBlocked())
	{
		if (onSubmitCallback_)
		{
			onSubmitCallback_(this);
		}
	}
}
