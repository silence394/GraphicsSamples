_AMD_SAMPLE_NAME = "HelloVulkan"

dofile ("../../premake/amd_premake_util.lua")

workspace (_AMD_SAMPLE_NAME)
    configurations { "Debug", "Release" }
    platforms { "x64" }
    location "../build"
    filename (_AMD_SAMPLE_NAME .. _AMD_VS_SUFFIX)
    startproject (_AMD_SAMPLE_NAME)

    filter "platforms:x64"
        system "Windows"
        architecture "x64"

project (_AMD_SAMPLE_NAME)
    kind "WindowedApp"
    language "C++"
    location "../build"
    filename (_AMD_SAMPLE_NAME .. _AMD_VS_SUFFIX)
    uuid "4ABD8B07-B672-04FD-3F67-FED3AB1BFB00"
    targetdir "../bin"
    objdir "../build/%{_AMD_SAMPLE_DIR_LAYOUT}"
    warnings "Extra"
    floatingpoint "Fast"

    -- Specify WindowsTargetPlatformVersion here for VS2015
    windowstarget (_AMD_WIN_SDK_VERSION)

    files { "../src/**.h", "../src/**.cpp" }
    links { "$(VULKAN_SDK)/lib/vulkan-1.lib" }
    includedirs { "$(VULKAN_SDK)/include" }

    defines { "_CRT_SECURE_NO_WARNINGS" }

    filter "configurations:Debug"
        defines { "WIN32", "_DEBUG", "DEBUG", "_WINDOWS" }
        flags { "Symbols", "FatalWarnings", "Unicode", "WinMain" }
        targetsuffix ("_Debug" .. _AMD_VS_SUFFIX)

    filter "configurations:Release"
        defines { "WIN32", "NDEBUG", "PROFILE", "_WINDOWS" }
        flags { "LinkTimeOptimization", "Symbols", "FatalWarnings", "Unicode", "WinMain" }
        targetsuffix ("_Release" .. _AMD_VS_SUFFIX)
        optimize "On"
