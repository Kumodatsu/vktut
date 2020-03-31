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
        vulkan_sdk .. "/include"
    }
    libdirs {
        vulkan_sdk .. "/lib"
    }
    links {
        "GLFW"
    }
    filter "system:windows"
        links {
            "vulkan-1"
        }
        disablewarnings {
            26812
        }
    filter "system:linux"
        links { 
            "vulkan",
            "dl",
            "pthread"
        }
    filter "configurations:Debug"
        runtime  "Debug"
        symbols  "on"
        optimize "off"
        defines {
            "KUMO_CONFIG_DEBUG"
        }
    filter "configurations:Profile"
        runtime  "Release"
        symbols  "on"
        optimize "on"
        defines {
            "KUMO_CONFIG_PROFILE"
        }
    filter "configurations:Release"
        runtime  "Release"
        symbols  "off"
        optimize "on"
        defines {
            "KUMO_CONFIG_RELEASE"
        }
