


add_executable(CudaSingleCompilation singleComp/singleCompilation.cu)
target_link_libraries(CudaSingleCompilation PRIVATE CUDA::cudart_static)
#set_target_properties(CudaSingleCompilation PROPERTIES CUDA_ARCHITECTURES "50")
#set_target_properties(CudaSingleCompilation PROPERTIES CUDA_ARCHITECTURES "70")


add_executable(CudaSplitHeader "wrapperComp/wrapperCompilation.cpp" wrapperComp/kernelImpl.cu wrapperComp/funcImpl.cu)
target_include_directories(CudaSplitHeader PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/wrapperComp)
target_link_libraries(CudaSplitHeader PRIVATE CUDA::cudart_static)
#set_target_properties(CudaSplitHeader PROPERTIES CUDA_ARCHITECTURES "50")
#set_target_properties(CudaSplitHeader PROPERTIES CUDA_ARCHITECTURES "70")


add_library(CudaSeperateCompilationDeviceLib STATIC deviceLib/deviceLibImpl.cu)
target_include_directories(CudaSeperateCompilationDeviceLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/deviceLib)
target_link_libraries(CudaSeperateCompilationDeviceLib PRIVATE CUDA::cudart_static)
set_target_properties(CudaSeperateCompilationDeviceLib PROPERTIES CUDA_SEPARABLE_COMPILATION ON)


add_library(CudaSeperateCompilationLib STATIC sepKernelComp/kernelCode.cu)
target_link_libraries(CudaSeperateCompilationLib PRIVATE CUDA::cudart_static CudaSeperateCompilationDeviceLib)
set_target_properties(CudaSeperateCompilationLib PROPERTIES CUDA_SEPARABLE_COMPILATION ON)


add_executable(CudaSeperateCompilation sepKernelComp/main.cpp)
target_include_directories(CudaSeperateCompilation PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/sepKernelComp)
target_link_libraries(CudaSeperateCompilation PRIVATE CUDA::cudart_static CudaSeperateCompilationLib)
#set_target_properties(CudaSplitHeader PROPERTIES CUDA_ARCHITECTURES "50")
#set_target_properties(CudaSplitHeader PROPERTIES CUDA_ARCHITECTURES "70")
