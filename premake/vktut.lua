local vulkan_sdk = os.getenv("VULKAN_SDK") or os.getenv("VK_SDK_PATH")
if vulkan_sdk == nil then
    print "Can't locate Vulkan SDK."
    return
end

project "VKTut"
    location      "../projects/VKTut"
    kind          "ConsoleApp"
    language      "C++"
    cppdialect    "C++17"
    staticruntime "on"
    systemversion "latest"
    pchheader     "Common.hpp"
    pchsource     "../projects/VKTut/src/Common.cpp"
    warnings      "extra"
    disablewarnings {
        26812
    }
    targetdir     "../output/bin/%{cfg.buildcfg}/%{prj.name}"
    objdir        "../output/obj/%{cfg.buildcfg}/%{prj.name}"
    defines {
        "GLFW_INCLUDE_NONE",
        "GLM_FORCE_RADIANS"
    }
    files {
        "../projects/%{prj.name}/src/**.h",
        "../projects/%{prj.name}/src/**.hpp",
        "../projects/%{prj.name}/src/**.cpp"
    }
    includedirs {
        "../projects/%{prj.name}/src",
        "../dependencies/glfw/include",
        "../dependencies/glm",
        vulkan_sdk .. "/Include"
    }
    libdirs {
        vulkan_sdk .. "/Lib"
    }
    links {
        "GLFW",
        "vulkan-1"
    }
    filter "configurations:Debug"
        runtime  "Debug"
        symbols  "on"
        optimize "off"
    filter "configurations:Profile"
        runtime  "Release"
        symbols  "on"
        optimize "on"
    filter "configurations:Release"
        runtime  "Release"
        symbols  "off"
        optimize "on"
