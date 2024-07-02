#pragma once
#include <RendererBase.h>

#include "nanovdb/util/CreateNanoGrid.h"
#include "owl/owl_host.h"

#include <CudaGpuTimers.h>

#include "RuntimeDataSet.h"
#include <FoveatedRendering.h>
#include "features/BackgroundColorFeature.h"
#include "features/ColorMapFeature.h"
#include "features/RenderSyncFeature.h"
#include "features/RenderTargetFeature.h"
#include "features/TransferFunctionFeature.h"
#include "..//nanoOutOfCore/OpenFileDialog.h"


namespace b3d::renderer
{
	struct NanoContext
	{
		OWLContext context;
		OWLRayGen rayGen;
		OWLRayGen rayGenFoveated;
		OWLMissProg missProgram;
		OWLGroup worldGeometryGroup;
		OWLLaunchParams launchParams;
	};

	class NanoRenderer final : public RendererBase
	{
	public:
		NanoRenderer()
		{
			renderTargetFeature_ = addFeature<RenderTargetFeature>("RenderTargets");
			colorMapFeature_ = addFeature<ColorMapFeature>("Color Filtering");
			transferFunctionFeature_ = addFeature<TransferFunctionFeature>("Transfer Function");
			backgroundColorFeature_ = addFeature<BackgroundColorFeature>(
				"Background Color", std::array<ColorRGB, 2>{ { { 0,0,0}, { 0,0,0 } } });
			foveatedFeature_ = addFeature<FoveatedRenderingFeature>();
			// renderSyncFeature_ = addFeatureWithDependency<RenderSyncFeature>({renderTargetFeature_, colorMapFeature_, transferFunctionFeature_, backgroundColorFeature_, foveatedFeature_},"Main Synchronization");
		}

		void addNanoVdb(std::filesystem::path pathToNanoVdb)
		{
			runtimeDataSet_.addNanoVdb(pathToNanoVdb);
		}

		void selectDataSet(int i )
		{
			runtimeDataSet_.select(i);
			
		}

	protected:
		auto onRender() -> void override;
		auto onInitialize() -> void override;
		auto onDeinitialize() -> void override;
		auto onGui() -> void override;

	private:
		auto prepareGeometry() -> void;

		NanoContext nanoContext_{};
		owl::AffineSpace3f trs_{};

		nanovdb::Map currentMap_{};

		CudaGpuTimers<100, 4> gpuTimers_{};

		RenderTargetFeature* renderTargetFeature_;
		// RenderSyncFeature* renderSyncFeature_;
		ColorMapFeature* colorMapFeature_;
		TransferFunctionFeature* transferFunctionFeature_;
		BackgroundColorFeature* backgroundColorFeature_;
		FoveatedRenderingFeature* foveatedFeature_;

		nano::RuntimeDataSet runtimeDataSet_{};
		nano::OpenFileDialog openFileDialog_{ nano::SelectMode::singleFile, { ".nvdb" } };//TODO: add multiselect mode
	};
} // namespace b3d::renderer
