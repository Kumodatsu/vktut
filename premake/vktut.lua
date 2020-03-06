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
        "GLFW_INCLUDE_NONE"
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
        "../dependencies/vulkan/Include"
    }
    libdirs {
        "../dependencies/vulkan/Lib"
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
