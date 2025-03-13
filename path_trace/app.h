#pragma once
#include "window.h"
#include "scene.h"
#include "graphics.h"

namespace w {
class App
{
public:
    App();
    ~App();

public:
    int run();

private:
    uint32_t ProcessEvents();

    void OnKeyPressed(const SDL_Event& event);
    void OnMouseMove(const SDL_Event& event);
    void OnWheel(const SDL_Event& event);

private:
    w::Swapchain CreateSwapchain();
    void InitImGui(std::span<wis::DescriptorBindingDesc> bindings);
    void InitResources();

    void Frame();
    void CopyToSwapchain();

    void CreateSizeDependentResources(uint32_t width, uint32_t height);
    void MakeTransitions();

private:
    void RenderUI();

private:
    int width = 0;
    int height = 0;

    w::Window window;
    w::Graphics gfx;
    w::Swapchain swapchain;

    wis::DescriptorStorage desc_storage;

    wis::CommandList command_list[w::flight_frames];
    wis::CommandList ui_command_list[w::flight_frames];
    wis::CommandList aux_command_list;

    wis::Texture uav_texture[w::flight_frames];
    wis::UnorderedAccessTexture uav_output[w::flight_frames];

    w::Scene scene;
};
} // namespace w