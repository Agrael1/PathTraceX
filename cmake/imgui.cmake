project(imgui CXX)

add_library(${PROJECT_NAME} STATIC)
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})
target_include_directories(
    ${PROJECT_NAME}
    PUBLIC
        "$<BUILD_INTERFACE:${IMGUI_SRC}>"
        $<INSTALL_INTERFACE:include>
)

target_sources(
    ${PROJECT_NAME}
    PRIVATE
        ${IMGUI_SRC}/imgui.cpp
        ${IMGUI_SRC}/imgui_demo.cpp
        ${IMGUI_SRC}/imgui_draw.cpp
        ${IMGUI_SRC}/imgui_tables.cpp
        ${IMGUI_SRC}/imgui_widgets.cpp
        ${IMGUI_SRC}/misc/cpp/imgui_stdlib.cpp
)

if(IMGUI_BUILD_DX12_BINDING)
    target_sources(${PROJECT_NAME} PRIVATE ${IMGUI_SRC}/backends/imgui_impl_dx12.cpp)
endif()

if(IMGUI_BUILD_SDL3_BINDING AND TARGET SDL3::SDL3)
    target_link_libraries(${PROJECT_NAME} PUBLIC SDL3::SDL3)
    target_sources(${PROJECT_NAME} PRIVATE ${IMGUI_SRC}/backends/imgui_impl_sdl3.cpp)
endif()

if(IMGUI_FREETYPE)
    find_package(freetype CONFIG REQUIRED)
    target_link_libraries(${PROJECT_NAME} PUBLIC freetype)
    target_sources(${PROJECT_NAME} PRIVATE ${IMGUI_SRC}/misc/freetype/imgui_freetype.cpp)
    target_compile_definitions(${PROJECT_NAME} PUBLIC IMGUI_ENABLE_FREETYPE)
endif()

if(IMGUI_USE_WCHAR32)
    target_compile_definitions(${PROJECT_NAME} PUBLIC IMGUI_USE_WCHAR32)
endif()

list(REMOVE_DUPLICATES BINDINGS_SOURCES)

