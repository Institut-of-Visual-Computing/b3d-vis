#include <cuda/std/cstddef>
#include <device_launch_parameters.h>
#include <optix_device.h>
#include <owl/owl.h>

#include <cuda/std/array>

#include <nanovdb/util/HDDA.h>
#include <nanovdb/util/Ray.h>
#include "SharedStructs.h"
#include "nanovdb/NanoVDB.h"
#include "owl/common/math/vec.h"

#include "owl/owl_device.h"

#include "FoveatedHelper.cuh"
#include "OptixHelper.cuh"
#include "SampleAccumulators.h"
#include "SamplerMapper.h"

using namespace b3d::renderer::nano;
using namespace owl;

extern "C" __constant__ LaunchParams optixLaunchParams;


struct PerRayData
{
	vec3f color;
	float alpha;
	bool isBackground{ false };
	float stepsScale{ 1.0 };
};

__device__ inline auto confine(const nanovdb::BBox<nanovdb::Coord>& bbox, nanovdb::Vec3f& sample) -> void
{

	auto iMin = nanovdb::Vec3f(bbox.min());
	auto iMax = nanovdb::Vec3f(bbox.max()) + nanovdb::Vec3f(1.0f);

	constexpr auto eps = 1e-7f;
	if (sample[0] < iMin[0])
	{
		sample[0] = iMin[0];
	}
	if (sample[1] < iMin[1])
	{
		sample[1] = iMin[1];
	}
	if (sample[2] < iMin[2])
	{
		sample[2] = iMin[2];
	}
	if (sample[0] >= iMax[0])
	{
		sample[0] = iMax[0] - fmaxf(1.0f, fabsf(sample[0])) * eps;
	}
	if (sample[1] >= iMax[1])
	{
		sample[1] = iMax[1] - fmaxf(1.0f, fabsf(sample[1])) * eps;
	}
	if (sample[2] >= iMax[2])
	{
		sample[2] = iMax[2] - fmaxf(1.0f, fabsf(sample[2])) * eps;
	}
}

__device__ inline auto confine(const nanovdb::BBox<nanovdb::Coord>& bbox, nanovdb::Vec3f& start, nanovdb::Vec3f& end)
	-> void
{
	confine(bbox, start);
	confine(bbox, end);
}

__host__ __device__ auto b3d::renderer::nano::colorMap(const float value) -> owl::vec3f
{
	if (optixLaunchParams.coloringInfo.coloringMode == ColoringMode::single)
	{
		return owl::vec3f{ optixLaunchParams.coloringInfo.singleColor };
	}
	else
	{
		const auto result =
			tex2D<float4>(optixLaunchParams.colorMaps, value, optixLaunchParams.coloringInfo.selectedColorMap);
		return owl::vec3f{ result.x, result.y, result.z };
	}
}

__host__ __device__ auto b3d::renderer::nano::transferMap(const float a) -> float
{
	return tex2D<float>(optixLaunchParams.transferFunctionTexture, a, 0.5f);
}

OPTIX_BOUNDS_PROGRAM(volumeBounds)
(const void* geometryData, owl::box3f& primitiveBounds, const int primitiveID)
{
	const auto& self = *static_cast<const GeometryData*>(geometryData);
	primitiveBounds = self.volume.indexBox;
	printf("%.2f ___ %.2f ___  %.2f \n", primitiveBounds.lower.x, primitiveBounds.lower.y, primitiveBounds.lower.z);
	printf("%.2f ___ %.2f ___  %.2f \n", primitiveBounds.upper.x, primitiveBounds.upper.y, primitiveBounds.upper.z);

	primitiveBounds = box3f{ { -1000.0f, -1000.0f, -1000.0f }, { 1000.0f, 1000.0f, 1000.0f } };
	// TODO: need better solution
}

OPTIX_RAYGEN_PROGRAM(hitCountRayGen)()
{
}


OPTIX_RAYGEN_PROGRAM(rayGeneration)()
{
	const auto& self = owl::getProgramData<RayGenerationData>();

	const auto& camera = optixLaunchParams.cameraData;
	const auto pixelId = owl::getLaunchIndex();

	const auto screen = (vec2f(pixelId) + vec2f(.5f)) / vec2f(self.frameBufferSize); //*2.0f -1.0f;

	owl::Ray ray;
	ray.origin = camera.pos;
	ray.direction = normalize(camera.dir_00 + screen.x * camera.dir_du + screen.y * camera.dir_dv);

	PerRayData prd;
	owl::traceRay(self.world, ray, prd);

	const auto bg1 = optixLaunchParams.bg.color1;
	const auto bg2 = optixLaunchParams.bg.color0;
	const auto pattern = (pixelId.x / 8) ^ (pixelId.y / 8);
	const auto bgColor = (pattern & 1) ? bg1 : bg2;

	const auto color = prd.isBackground ? bgColor : bgColor * (1.0f - prd.alpha) + prd.alpha * vec4f(prd.color, 1.0f);
	surf2Dwrite(owl::make_rgba(color), optixLaunchParams.surfacePointer, sizeof(uint32_t) * pixelId.x, pixelId.y);
}

OPTIX_RAYGEN_PROGRAM(rayGenerationFoveated)()
{
	const auto& self = owl::getProgramData<RayGenerationFoveatedData>();

	const auto foveal = self.foveal;
	const auto resolution = vec2f(self.frameBufferSize.x, self.frameBufferSize.y);
	/*const auto maxL = max(
		max(length((vec2f(1, 1) - foveal) * resolution),
			length((vec2f(1, -1) - foveal) * resolution)
		),
		max(length((vec2f(-1, 1) - foveal) * resolution),
			length((vec2f(-1, -1) - foveal) * resolution)
		)
	);
	const auto L = log(maxL * 0.5);*/


	const auto& camera = optixLaunchParams.cameraData;
	const auto pixelIndex = owl::getLaunchIndex();
	const auto scaleRatio = self.resolutionScaleRatio;

	/*
	const auto screen = (vec2f(pixelIndex) + vec2f(.5f)) / vec2f(self.frameBufferSize); //*2.0f -1.0f;


	const auto pq = screen * 2.0f - 1.0f - foveal;
	const auto lr = pow(log(length(pq * resolution * 0.5f)) / L, 4.0);
	constexpr auto pi2 = nanovdb::pi<float>()*2.0f;
	float theta = atan2f(pq.y * resolution.y, pq.x * resolution.x)/pi2 + (pq.y < 0.0f ? 1.0f : 0.0);
	float theta2 = atan2f(pq.y * resolution.y, -abs(pq.x) * resolution.x)/pi2 + (pq.y < 0.0f ? 1.0f : 0.0);

	const auto logCoord = vec2f(lr, theta) / scaleRatio;
	surf2Dwrite(owl::make_rgba(vec3f(logCoord.x, logCoord.y, 0.0f)), optixLaunchParams.surfacePointer, sizeof(uint32_t)
	* pixelIndex.x, pixelIndex.y); return;
	*/
	/*const auto uv = vec2f(pixelIndex) / resolution * scaleRatio;

	auto newPixelIndex = vec2f(pixelIndex);

	if (uv.x > 1.0 || uv.y > 1.0) {
		if (uv.y > 1.0 && uv.y < 1.0 + 1.0/resolution.y) {
			newPixelIndex -= resolution/ scaleRatio;
		} else {
			return;
		}
	}*/


	// auto screen = (vec2f(newPixelIndex) + vec2f(.5f)); //*2.0f -1.0f;
	auto screen = (vec2f(pixelIndex) + vec2f(.5f)); //*2.0f -1.0f;

	screen = logMap(scaleRatio, screen, foveal, resolution);
	// screen = inverseLogMap(scaleRatio, screen, foveal, resolution);
	/*surf2Dwrite(owl::make_rgba(vec3f(screen.x, screen.y, 0.0f)), optixLaunchParams.surfacePointer, sizeof(uint32_t) *
	pixelIndex.x, pixelIndex.y); return;*/
	screen /= vec2f(self.frameBufferSize);

	owl::Ray ray;
	ray.origin = camera.pos;
	ray.direction = normalize(camera.dir_00 + screen.x * camera.dir_du + screen.y * camera.dir_dv);

	screen = (vec2f(pixelIndex) + vec2f(.5f));
	const auto maxL =
		max(max(length((vec2f(1, 1) - foveal) * resolution), length((vec2f(1, -1) - foveal) * resolution)),
			max(length((vec2f(-1, 1) - foveal) * resolution), length((vec2f(-1, -1) - foveal) * resolution)));
	const float L = log(maxL * 0.5);
	auto uv = scaleRatio * screen / resolution;

	uv.x = pow(uv.x, 1.0f / 4.0f);

	screen /= vec2f(self.frameBufferSize);

	PerRayData prd;
	prd.stepsScale = exp((uv.x * L));
	owl::traceRay(self.world, ray, prd);

	const auto bg1 = optixLaunchParams.bg.color1;
	const auto bg2 = optixLaunchParams.bg.color0;
	const auto pattern = (pixelIndex.x / 8) ^ (pixelIndex.y / 8);
	const auto bgColor = (pattern & 1) ? bg1 : bg2;

	const auto color = prd.isBackground ? bgColor : bgColor * (1.0f - prd.alpha) + prd.alpha * vec4f(prd.color, 1.0f);
	surf2Dwrite(owl::make_rgba(color), optixLaunchParams.surfacePointer, sizeof(uint32_t) * pixelIndex.x, pixelIndex.y);
}

OPTIX_MISS_PROGRAM(miss)()
{
	auto& prd = owl::getPRD<PerRayData>();
	prd.isBackground = true;
}

OPTIX_CLOSEST_HIT_PROGRAM(nano_closestHit)()
{
	const auto* grid = reinterpret_cast<nanovdb::FloatGrid*>(optixLaunchParams.volume.grid);

	auto transform = cuda::std::array<float, 12>{};

	optixGetWorldToObjectTransformMatrix(transform.data());

	auto indexToWorldTransform =
		cuda::std::array<float, 9>{ { transform[0], transform[1], transform[2], transform[4], transform[5],
									  transform[6], transform[8], transform[9], transform[10] } };
	const auto translate = nanovdb::Vec3f{ transform[3], transform[7], transform[11] };

	const auto& accessor = grid->getAccessor();

	const auto rayOrigin = optixGetWorldRayOrigin();
	const auto rayDirection = optixGetWorldRayDirection();

	const auto t0 = optixGetRayTmax();
	const auto t1 = getPRD<float>();

	const auto rayWorld = nanovdb::Ray<float>(reinterpret_cast<const nanovdb::Vec3f&>(rayOrigin),
											  reinterpret_cast<const nanovdb::Vec3f&>(rayDirection));

	const auto startWorld = rayWorld(t0);
	const auto endWorld = rayWorld(t1);
	const auto a = nanovdb::Vec3f(startWorld[0], startWorld[1], startWorld[2]);
	const auto b = nanovdb::Vec3f(endWorld[0], endWorld[1], endWorld[2]);
	auto start = nanovdb::matMult(indexToWorldTransform.data(), a) + translate;
	auto end = nanovdb::matMult(indexToWorldTransform.data(), b) + translate;

	const auto& bbox = grid->indexBBox();
	confine(bbox, start, end);

	const auto direction = end - start;
	const auto length = direction.length();
	const auto ray = nanovdb::Ray<float>(start, direction / length, 0.0f, length);
	auto ijk = nanovdb::RoundDown<nanovdb::Coord>(ray.start());

	auto hdda = nanovdb::HDDA<nanovdb::Ray<float>>(ray, accessor.getDim(ijk, ray));

	auto result = vec4f{};
	auto& prd = owl::getPRD<PerRayData>();

	auto remapSample = [](const float value)
	{
		return optixLaunchParams.sampleRemapping.x +
			value / (optixLaunchParams.sampleRemapping.y - optixLaunchParams.sampleRemapping.x);
	};

	const auto steps = 10.0f * prd.stepsScale;
	const auto integrate = [&](auto sampleAccumulator)
	{
		sampleAccumulator.preAccumulate();

		const auto acc = grid->tree().getAccessor();

		while (hdda.step())
		{
			// TODO: can we remove step and update? Investigate examples in optix sdk
			const auto tt = hdda.next();
			ijk = hdda.voxel();
			const auto value = remapSample(accessor.getValue(nanovdb::Coord::Floor(ray(tt))));
			sampleAccumulator.accumulate(value);
			hdda.update(ray, accessor.getDim(ijk, ray));
		}

		sampleAccumulator.postAccumulate();
		return sampleAccumulator.getAccumulator();
	};

	switch (optixLaunchParams.sampleIntegrationMethod)
	{

	case SampleIntegrationMethod::transferIntegration:
		{
			auto sampleAccumulator = IntensityIntegration{};
			result = integrate(sampleAccumulator);
		}

		break;
	case SampleIntegrationMethod::maximumIntensityProjection:
		{
			auto sampleAccumulator = MaximumIntensityProjection{};
			result = integrate(sampleAccumulator);
		}

		break;
	case SampleIntegrationMethod::averageIntensityProjection:
		{
			auto sampleAccumulator = AverageIntensityProjection{};
			result = integrate(sampleAccumulator);
		}
		break;
	}

	prd.color = vec3f{ result.x, result.y, result.z };
	prd.alpha = result.w;
}

OPTIX_INTERSECT_PROGRAM(nano_intersection)()
{
	const auto* grid = reinterpret_cast<nanovdb::FloatGrid*>(optixLaunchParams.volume.grid);

	const auto rayOrigin = optixGetObjectRayOrigin();
	const auto rayDirection = optixGetObjectRayDirection();

	const auto& bbox = grid->worldBBox();
	auto t0 = optixGetRayTmin();
	auto t1 = optixGetRayTmax();
	const auto ray = nanovdb::Ray<float>(reinterpret_cast<const nanovdb::Vec3f&>(rayOrigin),
										 reinterpret_cast<const nanovdb::Vec3f&>(rayDirection), t0, t1);

	if (ray.intersects(bbox, t0, t1))
	{
		auto& t = getPRD<float>();
		t = t1;

		optixReportIntersection(fmaxf(t0, optixGetRayTmin()), 0);
	}
}
