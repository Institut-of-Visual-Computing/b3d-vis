#pragma once

#ifdef __cplusplus
#include <cstddef> 
#endif


#if defined(_MSC_VER)
#  define DLL_EXPORT __declspec(dllexport)
#  define DLL_IMPORT __declspec(dllimport)
#elif defined(__clang__) || defined(__GNUC__)
#  define DLL_EXPORT __attribute__((visibility("default")))
#  define DLL_IMPORT __attribute__((visibility("default")))
#else
#  define DLL_EXPORT
#  define DLL_IMPORT
#endif

#ifdef __cplusplus
#define NANOCUT_API extern "C" DLL_EXPORT
#else



#define NANOCUT_API
#endif
typedef enum nanoCutError_enum
{
	NANOCUT_OK = 0,
	NANOCUT_INVALIDE_ARGUMENT = 1,
	NANOCUT_INTERNAL_ERROR = 2
} ncResult;

NANOCUT_API ncResult ncConvertFitsToNanoVdbWithMask(const char* fitsFile, const char* muskFile, const char* destinationNanoVdbFile);
