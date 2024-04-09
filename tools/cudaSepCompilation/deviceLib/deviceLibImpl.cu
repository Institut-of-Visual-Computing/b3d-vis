#include "deviceLib.cuh"

#include "cuda_runtime.h"

#include <device_launch_parameters.h>

__device__ int g[N];

__device__ auto bar(void) -> void
{

	g[threadIdx.x]++;
}
