#include "kernelHeader.cuh"

__device__ __host__ auto mul(const unsigned a, const unsigned b) -> unsigned
{
	return a * b;
}
