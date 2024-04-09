#include <cstdio>

#include "kernelHeader.cuh"

auto main(const int argc, char** argv) -> int
{
	printf("1 + 1 = %d\n", add(1, 1));
	printf("1 * 1 = %d\n", mul(1, 1));
	waitKernelCall(1, 1);
}
