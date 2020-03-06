local dir = "../dependencies/glfw"

project "GLFW"
    location      (dir)
    kind          "StaticLib"
    language      "C"
    staticruntime "on"
    systemversion "latest"
    warnings      "off"
    targetdir     "../output/bin/%{cfg.buildcfg}/%{prj.name}"
    objdir        "../output/obj/%{cfg.buildcfg}/%{prj.name}"
    files {
        dir .. "/include/GLFW/glfw3.h",
        dir .. "/include/GLFW/glfw3native.h",
        dir .. "/src/glfw_config.h",
        dir .. "/src/context.c",
        dir .. "/src/init.c",
        dir .. "/src/input.c",
        dir .. "/src/monitor.c",
        dir .. "/src/vulkan.c",
        dir .. "/src/window.c"
    }
    filter "system:windows"
        files {
            dir .. "/src/win32_init.c",
            dir .. "/src/win32_joystick.c",
            dir .. "/src/win32_monitor.c",
            dir .. "/src/win32_time.c",
            dir .. "/src/win32_thread.c",
            dir .. "/src/win32_window.c",
            dir .. "/src/wgl_context.c",
            dir .. "/src/egl_context.c",
            dir .. "/src/osmesa_context.c"
        }
        defines {
            "_GLFW_WIN32"
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
