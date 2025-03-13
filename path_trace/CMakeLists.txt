project(PathTracer LANGUAGES CXX)


add_executable(${PROJECT_NAME} 
	"entry_main.cpp"
	"app.cpp" 
	"app.h" 
	"window.h" 
	"window.cpp" 
	"consts.h" 
	 
	"imgui/imgui_impl_wisdom.cpp" 
	"imgui/imgui_impl_wisdom.h" 
	"scene.h" 
	"scene.cpp" 
	"sphere.h"
	"graphics.h"
	"graphics.cpp"
	"sphere.cpp"
)

set_target_properties(${PROJECT_NAME} PROPERTIES 
	CXX_STANDARD 23
	CXX_STANDARD_REQUIRED ON
	CXX_EXTENSIONS OFF
)
target_link_libraries(${PROJECT_NAME} 
	PRIVATE 
		wis::wisdom-headers
		wis::debug-headers
		wis::raytracing-headers
		wis::platform-headers
		wis::extended-allocation-headers

		SDL3::SDL3
		imgui::imgui
)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		$<TARGET_RUNTIME_DLLS:${PROJECT_NAME}>
		$<TARGET_FILE_DIR:${PROJECT_NAME}>
)

add_custom_target(shaders)

# create directory for shaders
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/shaders")


WIS_COMPILE_SHADER(
	TARGET shaders
	SHADER "shaders/imgui.vs.hlsl"
	OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/shaders/imgui.vs"
)
WIS_COMPILE_SHADER(
	TARGET shaders
	SHADER "shaders/imgui.ps.hlsl"
	OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/shaders/imgui.ps"
)
WIS_COMPILE_SHADER(
	TARGET shaders
	SHADER "shaders/pathtrace.lib.hlsl"
	OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/shaders/pathtrace.lib"
	SHADER_MODEL "6.3"
	TYPE "lib"
)
WIS_COMPILE_SHADER(
	TARGET shaders
	SHADER "shaders/hit.lib.hlsl"
	OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/shaders/hit.lib"
	SHADER_MODEL "6.3"
	TYPE "lib"
)
WIS_INSTALL_DEPS(${PROJECT_NAME})

add_dependencies(${PROJECT_NAME} shaders)