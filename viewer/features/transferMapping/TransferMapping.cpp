#include "TransferMapping.h"

#include <cuda_gl_interop.h>
#include <owl/common.h>
#include <owl/helper/cuda.h>
#include <ranges>
#include <stb_image.h>

#include <RenderData.h>

auto TransferMapping::initializeResources() -> void
{

	GL_CALL(glGenTextures(1, &colorMapResources_.colorMapTexture));
	GL_CALL(glBindTexture(GL_TEXTURE_2D, colorMapResources_.colorMapTexture));

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);


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

		const auto result = stbi_info(filePath.string().c_str(), &x, &y, &n);

		auto data = stbi_loadf(filePath.string().c_str(), &x, &y, &n, 0);

		GL_CALL(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, x, y, 0, GL_RGBA, GL_FLOAT, data));

		stbi_image_free(data);

		colorMapResources_.colorMapTextureExtent =
			b3d::renderer::Extent{ static_cast<uint32_t>(x), static_cast<uint32_t>(y), 1 };
	}
	else
	{
		GL_CALL(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 512, 1, 0, GL_RGBA, GL_FLOAT, nullptr));
		colorMapResources_.colorMapTextureExtent = b3d::renderer::Extent{ 512, 1, 1 };
	}

	const auto colorMapTextureName = std::string{ "ColorMap" };
	GL_CALL(glObjectLabel(GL_TEXTURE, colorMapResources_.colorMapTexture, colorMapTextureName.length() + 1,
						  colorMapTextureName.c_str()));

	OWL_CUDA_CHECK(cudaGraphicsGLRegisterImage(&colorMapResources_.cudaGraphicsResource,
											   colorMapResources_.colorMapTexture, GL_TEXTURE_2D,
											   cudaGraphicsRegisterFlagsTextureGather));

	// Transfer function
	GL_CALL(glGenTextures(1, &transferFunctionResources_.transferFunctionTexture));
	GL_CALL(glBindTexture(GL_TEXTURE_2D, transferFunctionResources_.transferFunctionTexture));

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);


	auto initBufferData = std::array<float, 512>{};

	std::ranges::fill(initBufferData, 1);
	GL_CALL(glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, 512, 1, 0, GL_RED, GL_FLOAT, initBufferData.data()));

	const auto transferFunctionBufferName = std::string{ "TransferFunctionTexture" };
	GL_CALL(glObjectLabel(GL_TEXTURE, transferFunctionResources_.transferFunctionTexture,
						  transferFunctionBufferName.length() + 1, transferFunctionBufferName.c_str()));

	OWL_CUDA_CHECK(cudaGraphicsGLRegisterImage(
		&transferFunctionResources_.cudaGraphicsResource, transferFunctionResources_.transferFunctionTexture,
		GL_TEXTURE_2D, cudaGraphicsRegisterFlagsTextureGather | cudaGraphicsRegisterFlagsWriteDiscard));
}

auto TransferMapping::deinitializeResources() -> void
{
	OWL_CUDA_CHECK(cudaGraphicsUnregisterResource(transferFunctionResources_.cudaGraphicsResource));
	OWL_CUDA_CHECK(cudaGraphicsUnregisterResource(colorMapResources_.cudaGraphicsResource));

	GL_CALL(glDeleteTextures(1, &colorMapResources_.colorMapTexture));
	GL_CALL(glDeleteTextures(1, &transferFunctionResources_.transferFunctionTexture));
}

auto TransferMapping::updateRenderingData(b3d::renderer::RenderingDataWrapper& renderingData) -> void
{
	renderingData.data.colorMapTexture.extent = colorMapResources_.colorMapTextureExtent;
	renderingData.data.colorMapTexture.nativeHandle = reinterpret_cast<void*>(colorMapResources_.colorMapTexture);
	renderingData.data.colorMapTexture.target = colorMapResources_.cudaGraphicsResource;
	renderingData.data.coloringInfo =
		b3d::renderer::ColoringInfo{ b3d::renderer::ColoringMode::single, owl::vec4f{ 1, 1, 1, 1 },
									 colorMapResources_.colorMap.firstColorMapYTextureCoordinate };

	renderingData.data.colorMapInfos =
		b3d::renderer::ColorMapInfos{ &colorMapResources_.colorMap.colorMapNames,
									  colorMapResources_.colorMap.colorMapCount,
									  colorMapResources_.colorMap.firstColorMapYTextureCoordinate,
									  colorMapResources_.colorMap.colorMapHeightNormalized };

	renderingData.data.transferFunctionTexture.extent = { 512, 1, 1 };
	renderingData.data.transferFunctionTexture.target = transferFunctionResources_.cudaGraphicsResource;
	renderingData.data.transferFunctionTexture.nativeHandle =
		reinterpret_cast<void*>(transferFunctionResources_.transferFunctionTexture);
}