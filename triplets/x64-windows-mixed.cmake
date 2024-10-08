set(VCPKG_TARGET_ARCHITECTURE x64)

if(${PORT} MATCHES "openvdb|blosc|boost|imath|tbb|openexr|tinyxml2|tracy|imgui")
	set(VCPKG_CRT_LINKAGE dynamic)
	set(VCPKG_LIBRARY_LINKAGE dynamic)
else()
	set(VCPKG_CRT_LINKAGE static)
	set(VCPKG_LIBRARY_LINKAGE static)
endif()
