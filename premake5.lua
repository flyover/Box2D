-- Box2D premake5 script.
-- https://premake.github.io/

workspace 'Box2D'
	configurations { 'Debug', 'Release' }
	startproject 'Testbed'
	location 'Build'
	symbols 'On'
	warnings 'Extra'

    filter 'system:linux'
        platforms { 'x86_64' }
        cppdialect 'C++11'
    filter 'system:macosx'
        platforms { 'x86_64' }
    filter 'system:windows'
        platforms { 'x86', 'x86_64' }
        defaultplatform 'x86_64'
		defines { '_CRT_SECURE_NO_WARNINGS' }
	filter {}

	filter 'configurations:Debug'
	 	defines { 'DEBUG' }
		optimize 'Off'
	filter 'configurations:Release'
		defines { 'NDEBUG' }
		optimize 'On'
	filter {}

project 'Box2D'
	kind 'StaticLib'
	files { 'Box2D/**' }
	includedirs { '.' }

project 'HelloWorld'
	kind 'ConsoleApp'
	files { 'HelloWorld/HelloWorld.cpp' }
	includedirs { '.' }
	links { 'Box2D' }

project 'Testbed'
	kind 'ConsoleApp'
	debugdir 'Testbed'
	warnings 'Default'
	includedirs { '.', 'Testbed/glfw/deps', 'Testbed/imgui' }

	files
	{
		'Testbed/Framework/*',
		'Testbed/Tests/*',
		'Testbed/glfw/src/internal.h',
		'Testbed/glfw/src/glfw_config.h',
		'Testbed/glfw/src/glfw3.h',
		'Testbed/glfw/src/glfw3native.h',
		'Testbed/glfw/src/context.c',
		'Testbed/glfw/src/init.c',
		'Testbed/glfw/src/input.c',
		'Testbed/glfw/src/monitor.c',
		'Testbed/glfw/src/vulkan.c',
		'Testbed/glfw/src/window.c',
		'Testbed/imgui/*'
	}

    filter { 'system:windows' }
		defines { '_GLFW_WIN32' }
		files
		{ 
			'Testbed/glfw/deps/*',
			'Testbed/glfw/src/win32_platform.h',
			'Testbed/glfw/src/win32_joystick.h',
			'Testbed/glfw/src/wgl_context.h',
			'Testbed/glfw/src/egl_context.h',
			'Testbed/glfw/src/win32_init.c',
			'Testbed/glfw/src/win32_joystick.c',
			'Testbed/glfw/src/win32_monitor.c',
			'Testbed/glfw/src/win32_time.c',
			'Testbed/glfw/src/win32_tls.c',
			'Testbed/glfw/src/win32_window.c',
			'Testbed/glfw/src/wgl_context.c',
			'Testbed/glfw/src/egl_context.c'
		}
		links { 'Box2D', 'opengl32', 'winmm' }

    filter { 'system:macosx' }
		defines { '_GLFW_COCOA' }
		files
		{
			'Testbed/glfw/src/cocoa_platform.h',
			'Testbed/glfw/src/iokit_joystick.h',
			'Testbed/glfw/src/posix_tls.h',
			'Testbed/glfw/src/nsgl_context.h',
			'Testbed/glfw/src/egl_context.h',
			'Testbed/glfw/src/cocoa_init.m',
			'Testbed/glfw/src/cocoa_joystick.m',
			'Testbed/glfw/src/cocoa_monitor.m',
			'Testbed/glfw/src/cocoa_window.m',
			'Testbed/glfw/src/cocoa_time.c',
			'Testbed/glfw/src/posix_tls.c',
			'Testbed/glfw/src/nsgl_context.m',
			'Testbed/glfw/src/egl_context.c'
		}
		--defines { 'GLFW_INCLUDE_GLCOREARB' }
		links
		{
			'Box2D',
			'OpenGL.framework',
			'Cocoa.framework',
			'IOKit.framework',
			'CoreFoundation.framework',
			'CoreVideo.framework'
		}
    
    filter { 'system:linux' }
		defines { '_GLFW_X11' }
		files
		{
			'Testbed/glfw/deps/*',
			'Testbed/glfw/src/x11_platform.h',
			'Testbed/glfw/src/xkb_unicode.h',
			'Testbed/glfw/src/linux_joystick.h',
			'Testbed/glfw/src/posix_time.h',
			'Testbed/glfw/src/posix_tls.h',
			'Testbed/glfw/src/glx_context.h',
			'Testbed/glfw/src/egl_context.h',
			'Testbed/glfw/src/x11_init.c',
			'Testbed/glfw/src/x11_monitor.c',
			'Testbed/glfw/src/x11_window.c',
			'Testbed/glfw/src/glx_context.h',
			'Testbed/glfw/src/glx_context.c',
			'Testbed/glfw/src/glext.h',
			'Testbed/glfw/src/xkb_unicode.c',
			'Testbed/glfw/src/linux_joystick.c',
			'Testbed/glfw/src/posix_time.c',
			'Testbed/glfw/src/posix_tls.c',
			'Testbed/glfw/src/glx_context.c',
			'Testbed/glfw/src/egl_context.c'
		}
		links
		{
			'Box2D',
			'GL',
			'X11',
			'Xrandr',
			'Xinerama',
			'Xcursor',
			'pthread',
			'dl'
		}
	
	filter {}
