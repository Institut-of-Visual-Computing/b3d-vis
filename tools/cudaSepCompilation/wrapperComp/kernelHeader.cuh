#pragma once

#include "cuda_runtime.h"
#include "vector_types.h"

// Must be marked as inline if header only?
inline __device__ __host__ auto add(const unsigned a, const unsigned b) -> unsigned
{
	return a + b;
}

__device__ __host__ auto mul(const unsigned a, const unsigned b) -> unsigned;

void waitKernelCall(dim3 gridDim, dim3 blockDim);
