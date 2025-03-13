#include "app.h"
#include "imgui/imgui_impl_wisdom.h"
#include <filesystem>
#include <fstream>

w::App::App()
    : window("Path Tracing", 1280, 720)
    , gfx(window.GetPlatformExtension())
    , swapchain(CreateSwapchain())
    , scene(gfx)
{
    wis::Result result = wis::success;
    InitResources();

    auto [w, h] = window.PixelSize();
    CreateSizeDependentResources(uint32_t(w), uint32_t(h));
    scene.Bind(gfx, desc_storage);
}

w::App::~App()
{
    swapchain.Throttle();
    ImGui_ImplWisdom_Shutdown();
}

int w::App::run()
{
    float dt = 1 / 60.0f;

    while (ProcessEvents()) {
        uint32_t frame_index = swapchain.CurrentFrame();

        ImGui_ImplWisdom_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        Frame();

        RenderUI();
        ImGui::Render();

        auto& uicl = ui_command_list[frame_index];
        auto& tex = swapchain.GetTexture(frame_index);
        auto res = uicl.Reset();

        if (scene.GammaCorrection()) {
            RenderToSwapchain();
        } else {
            CopyToSwapchain();
        }

        wis::DX12RenderPassRenderTargetDesc rt_desc{
            .target = swapchain.GetRenderTarget(frame_index),
            .load_op = wis::LoadOperation::Load,
            .clear_value = { 0.0f, 0.0f, 0.0f, 1.0f }
        };
        wis::RenderPassDesc rpd{
            .target_count = 1,
            .targets = &rt_desc,
        };
        uicl.BeginRenderPass(rpd);

        ImGui_ImplWisdom_RenderDrawData(ImGui::GetDrawData(), uicl);

        wis::TextureBarrier barrier_out{
            .sync_before = wis::BarrierSync::RenderTarget,
            .sync_after = wis::BarrierSync::Draw,
            .access_before = wis::ResourceAccess::RenderTarget,
            .access_after = wis::ResourceAccess::Common,
            .state_before = wis::TextureState::RenderTarget,
            .state_after = wis::TextureState::Present
        };
        uicl.TextureBarrier(barrier_out, tex);

        uicl.EndRenderPass();
        uicl.Close();

        gfx.ExecuteCommandLists({ command_list[frame_index], uicl });
        swapchain.Present(gfx);
    }

    return 0;
}

uint32_t w::App::ProcessEvents()
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL3_ProcessEvent(&event);
        switch (event.type) {
        case SDL_EVENT_QUIT:
            swapchain.Throttle();
            return 0;
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: {
            swapchain.Throttle();
            swapchain.Resize(gfx, event.window.data1, event.window.data2);
            CreateSizeDependentResources(event.window.data1, event.window.data2);
            break;
        }
        case SDL_EVENT_KEY_DOWN:
            if (ImGui::GetIO().WantCaptureKeyboard) {
                break;
            }
            OnKeyPressed(event);
            break;
        case SDL_EVENT_MOUSE_MOTION:
            if (ImGui::GetIO().WantCaptureMouse) {
                break;
            }
            OnMouseMove(event);
            break;
        case SDL_EVENT_MOUSE_WHEEL:
            if (ImGui::GetIO().WantCaptureMouse) {
                break;
            }
            OnWheel(event);
            break;
        }
    }
    return 1;
}

void w::App::OnKeyPressed(const SDL_Event& event)
{
    switch (event.key.key) {
    case SDLK_ESCAPE:
        window.PostQuit();
        break;
    }
}

void w::App::OnMouseMove(const SDL_Event& event)
{
    if (event.motion.state & SDL_BUTTON_LMASK) {
        scene.RotateCamera(float(event.motion.xrel), float(event.motion.yrel));
    }
}

void w::App::OnWheel(const SDL_Event& event)
{
    scene.ZoomCamera(float(event.wheel.y));
}

w::Swapchain w::App::CreateSwapchain()
{
    wis::Result result = wis::success;
    auto [w, h] = window.PixelSize();

    return w::Swapchain{ gfx.GetDevice(),
                         window.CreateSwapchain(result, gfx.GetDevice(), gfx.GetMainQueue()),
                         uint32_t(w), uint32_t(h) };
}

void w::App::InitImGui(std::span<wis::DescriptorBindingDesc> bindings)
{
    wis::Result result = wis::success;
    auto vertex_shader = LoadShader("shaders/imgui.vs");
    auto vs = gfx.device.CreateShader(result, vertex_shader.data(), uint32_t(vertex_shader.size()));
    auto pixel_shader = LoadShader("shaders/imgui.ps");
    auto ps = gfx.device.CreateShader(result, pixel_shader.data(), uint32_t(pixel_shader.size()));

    ImGui_ImplWisdom_InitInfo init_info{
        .extensions = nullptr,
        .device = &gfx.device,
        .allocator = &gfx.allocator,
        .command_queue = &gfx.main_queue,
        .desc_storage = &desc_storage,

        .vertex_shader = vs,
        .pixel_shader = ps,

        .descriptor_bindings = bindings.data(),
        .descriptor_bindings_count = uint32_t(bindings.size()),

        .rtv_format = w::swap_format,
        .dsv_format = wis::DataFormat::Unknown,

        .srv_descriptor_offset = 0,
        .srv_descriptor_binding = 0,
        .sampler_descriptor_offset = 0,
        .sampler_descriptor_binding = 1,
        .frames_in_flight_count = w::swap_frames,
        .queue_type = wis::QueueType::Graphics
    };
    ImGui_ImplWisdom_Init(&init_info);
}

void w::App::InitResources()
{
    wis::Result result = wis::success;
    auto& device = gfx.GetDevice();

    uint32_t requirements_count = 0;
    ImGui_ImplWisdom_DescriptorRequirement* reqs =
            ImGui_ImplWisdom_GetDescriptorRequirements(&requirements_count);
    std::span<ImGui_ImplWisdom_DescriptorRequirement> requirements{ reqs, requirements_count };

    wis::DescriptorBindingDesc bindings[] = {
        { wis::DescriptorType::Texture, 1, 1, 0 },
        { wis::DescriptorType::Sampler, 2, 1, 0 },
        { wis::DescriptorType::RWTexture, 3, 1, 2 },
        { wis::DescriptorType::AccelerationStructure, 4, 1, 2 },
        { wis::DescriptorType::Buffer, 5, 2, 2 },
    };

    for (auto& req : requirements) {
        switch (req.type) {
        case wis::DescriptorType::Texture:
            bindings[0].binding_count += req.count;
            break;
        case wis::DescriptorType::Sampler:
            bindings[1].binding_count += req.count;
            break;
        }
    }

    desc_storage = gfx.device.CreateDescriptorStorage(result, bindings, uint32_t(std::size(bindings)));
    InitImGui(bindings);

    for (uint32_t i = 0; i < w::swap_frames; i++) {
        command_list[i] = gfx.device.CreateCommandList(result, wis::QueueType::Graphics);
        ui_command_list[i] = gfx.device.CreateCommandList(result, wis::QueueType::Graphics);
    }
    aux_command_list = gfx.device.CreateCommandList(result, wis::QueueType::Graphics);

    scene.CreatePipeline(gfx, bindings);

    // Load shaders
    auto filter_code = w::LoadShader("shaders/filter.vs");
    wis::Shader filter_vs = device.CreateShader(result, filter_code.data(), filter_code.size());

    filter_code = w::LoadShader("shaders/filter.ps");
    wis::Shader filter_ps = device.CreateShader(result, filter_code.data(), filter_code.size());

    wis::GraphicsPipelineDesc filter_pipeline_desc{
        .root_signature = scene.root,
        .shaders = {
                .vertex = filter_vs,
                .pixel = filter_ps,
        },
        .attachments = {
                .attachment_formats = { w::swap_format },
                .attachments_count = 1,
        },
    };

    filter_pipeline = device.CreateGraphicsPipeline(result, filter_pipeline_desc);
}

void w::App::Frame()
{
    uint32_t frame_index = swapchain.CurrentFrame();
    auto& cmd = command_list[frame_index];
    cmd.Reset();
    scene.RenderScene(gfx, cmd, desc_storage, frame_index);
    cmd.Close();
}

void w::App::CopyToSwapchain()
{
    uint32_t frame_index = swapchain.CurrentFrame();
    auto& cmd = ui_command_list[frame_index];
    auto& uav_tex = uav_texture[frame_index];
    auto& swap_tex = swapchain.GetTexture(frame_index);

    wis::TextureBarrier2 barriers_in[]{
        { .barrier = { .sync_before = wis::BarrierSync::Raytracing,
                       .sync_after = wis::BarrierSync::Copy,
                       .access_before = wis::ResourceAccess::UnorderedAccess,
                       .access_after = wis::ResourceAccess::CopySource,
                       .state_before = wis::TextureState::UnorderedAccess,
                       .state_after = wis::TextureState::CopySource },
          .texture = uav_tex },
        { .barrier = { .sync_before = wis::BarrierSync::None,
                       .sync_after = wis::BarrierSync::Copy,
                       .access_before = wis::ResourceAccess::NoAccess,
                       .access_after = wis::ResourceAccess::CopyDest,
                       .state_before = wis::TextureState::Present,
                       .state_after = wis::TextureState::CopyDest },
          .texture = swap_tex }
    };
    cmd.TextureBarriers(barriers_in, std::size(barriers_in));

    wis::TextureCopyRegion region{
        .src = {
                .size = { uint32_t(width), uint32_t(height), 1 },
                .format = w::swap_format,
        },
        .dst = {
                .size = { uint32_t(width), uint32_t(height), 1 },
                .format = w::swap_format,
        },
    };
    cmd.CopyTexture(uav_tex, swap_tex, &region, 1);

    wis::TextureBarrier2 barriers_out[]{
        { .barrier = { .sync_before = wis::BarrierSync::Copy,
                       .sync_after = wis::BarrierSync::Raytracing,
                       .access_before = wis::ResourceAccess::CopySource,
                       .access_after = wis::ResourceAccess::UnorderedAccess,
                       .state_before = wis::TextureState::CopySource,
                       .state_after = wis::TextureState::UnorderedAccess },
          .texture = uav_tex },
        { .barrier = { .sync_before = wis::BarrierSync::Copy,
                       .sync_after = wis::BarrierSync::RenderTarget,
                       .access_before = wis::ResourceAccess::CopyDest,
                       .access_after = wis::ResourceAccess::RenderTarget,
                       .state_before = wis::TextureState::CopyDest,
                       .state_after = wis::TextureState::RenderTarget },
          .texture = swap_tex }
    };
    cmd.TextureBarriers(barriers_out, std::size(barriers_out));
}

void w::App::RenderToSwapchain()
{
    uint32_t frame_index = swapchain.CurrentFrame();
    auto& cmd = ui_command_list[frame_index];
    auto& uav_tex = uav_texture[frame_index];
    auto& swap_tex = swapchain.GetTexture(frame_index);

    wis::TextureBarrier2 barriers_in[]{
        { .barrier = { .sync_before = wis::BarrierSync::None,
                       .sync_after = wis::BarrierSync::RenderTarget,
                       .access_before = wis::ResourceAccess::NoAccess,
                       .access_after = wis::ResourceAccess::RenderTarget,
                       .state_before = wis::TextureState::Present,
                       .state_after = wis::TextureState::RenderTarget },
          .texture = swap_tex }
    };
    cmd.TextureBarriers(barriers_in, std::size(barriers_in));

    wis::RenderPassRenderTargetDesc rprtd{
        .target = swapchain.GetRenderTarget(frame_index),
        .load_op = wis::LoadOperation::DontCare,
    };
    wis::RenderPassDesc rpd{
        .target_count = 1,
        .targets = &rprtd,
    };
    cmd.BeginRenderPass(rpd);

    cmd.SetPipelineState(filter_pipeline);
    cmd.SetRootSignature(scene.root);
    cmd.SetDescriptorStorage(desc_storage);
    cmd.SetPushConstants(&frame_index, 1, 0, wis::ShaderStages::All);
    cmd.IASetPrimitiveTopology(wis::PrimitiveTopology::TriangleList);
    cmd.RSSetScissor({ 0, 0, width, height });
    cmd.RSSetViewport({ 0.0f, 0.0f, float(width), float(height), 0.0f, 1.0f });
    cmd.DrawInstanced(3);
    cmd.EndRenderPass();
}

void w::App::CreateSizeDependentResources(uint32_t width, uint32_t height)
{
    using namespace wis; // for flag operators
    wis::Result result = wis::success;

    if (width == 0 || height == 0) {
        return;
    }

    if (width <= this->width && height <= this->height) {
        this->width = width;
        this->height = height;
        scene.UpdateDispatch(width, height);
        return;
    }

    this->width = width;
    this->height = height;
    scene.UpdateDispatch(width, height);

    // Create UAV texture
    wis::TextureDesc desc{
        .format = w::swap_format,
        .size = { width, height, 1 },
        .usage = wis::TextureUsage::CopySrc | wis::TextureUsage::UnorderedAccess,
    };
    // Create UAV output
    wis::UnorderedAccessDesc uav_desc{
        .format = w::swap_format,
        .view_type = wis::TextureViewType::Texture2D,
        .subresource_range = { 0, 1, 0, 1 },
    };

    for (uint32_t i = 0; i < w::flight_frames; i++) {
        uav_texture[i] = gfx.allocator.CreateTexture(result, desc);
        uav_output[i] = gfx.device.CreateUnorderedAccessTexture(result, uav_texture[i], uav_desc);
        desc_storage.WriteRWTexture(2, i, uav_output[i]);
    }

    MakeTransitions();
}

void w::App::MakeTransitions()
{
    auto& cmd = aux_command_list;
    std::ignore = cmd.Reset();
    // Transition UAV texture to UAV state
    wis::TextureBarrier2 barriers[w::flight_frames] = {
        { .barrier = { .sync_before = wis::BarrierSync::None,
                       .sync_after = wis::BarrierSync::None,
                       .access_before = wis::ResourceAccess::NoAccess,
                       .access_after = wis::ResourceAccess::NoAccess,
                       .state_before = wis::TextureState::Undefined,
                       .state_after = wis::TextureState::UnorderedAccess },
          .texture = uav_texture[0] }
    };
    for (uint32_t i = 1; i < w::flight_frames; i++) {
        barriers[i] = barriers[0];
        barriers[i].texture = uav_texture[i];
    }

    cmd.TextureBarriers(barriers, std::size(barriers));
    cmd.Close();

    gfx.ExecuteCommandLists({ cmd });
    gfx.WaitForGpu();
}

void w::App::RenderUI()
{
    scene.RenderUI();
}