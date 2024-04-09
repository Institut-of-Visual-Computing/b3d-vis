
#include <cstdio>

#include "device_launch_parameters.h"

#include "cuda_runtime.h"

__device__ __host__ auto add(const unsigned a, const unsigned b) -> unsigned
{
	return a + b;
}

__global__ void waitKernel()
{
#if __CUDA_ARCH__ >= 700
	for (int i = 0; i < 1000; i++)
		__nanosleep(1000000U); // ls
#else
	const auto x = blockIdx.x * blockDim.x + threadIdx.x;
	const auto y = blockIdx.y * blockDim.y + threadIdx.y;
	if (add(x,y) == 0)
		printf(">>> __CUDA_ARCH__ must be 7.0 or higher!\n");
#endif
}

auto main(const int argc, char** argv) -> int
{
	printf("1 + 1 = %d\n", add(1, 1));
	waitKernel<<<1, 1>>>();
}
