
#include "cuda_runtime.h"
#include "device_launch_parameters.h"

#include "deviceLib.cuh"

#include "cstdio"

__device__ __host__ auto add(const unsigned a, const unsigned b) -> unsigned
{
	return a + b;
}

__global__ void waitKernel()
{
	
	bar();
#if __CUDA_ARCH__ >= 700
	for (int i = 0; i < 1000; i++)
		__nanosleep(1000000U); // ls
#else
	const auto x = blockIdx.x * blockDim.x + threadIdx.x;
	const auto y = blockIdx.y * blockDim.y + threadIdx.y;
	if (add(x, y) == 0)
		printf(">>> __CUDA_ARCH__ must be 7.0 or higher!\n");
#endif
}

void waitKernelCall(dim3 gridDim, dim3 blockDim)
{
	waitKernel<<<gridDim, blockDim>>>();
}
