// dear imgui: Renderer Backend for Wisdom
// This needs to be used along with a Platform Backend (e.g. Win32)

// Implemented features:
//  [X] Renderer: User texture binding. Use 'D3D12_GPU_DESCRIPTOR_HANDLE' as ImTextureID. Read the FAQ about ImTextureID!
//  [X] Renderer: Large meshes support (64k+ vertices) even with 16-bit indices (ImGuiBackendFlags_RendererHasVtxOffset).
//  [X] Renderer: Expose selected render state for draw callbacks to use. Access in '(ImGui_ImplXXXX_RenderState*)GetPlatformIO().Renderer_RenderState'.

// The aim of imgui_impl_dx12.h/.cpp is to be usable in your engine without any modification.
// IF YOU FEEL YOU NEED TO MAKE ANY CHANGE TO THIS CODE, please share them and your feedback at https://github.com/ocornut/imgui/

// You can use unmodified imgui_impl_* files in your project. See examples/ folder for examples of using this.
// Prefer including the entire imgui/ repository into your project (either as a copy or as a submodule), and only build the backends you need.
// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

// CHANGELOG
// (minor and older changes stripped away, please see git history for details)
//  2025-01-15: DirectX12: Texture upload use the command queue provided in ImGui_ImplDX12_InitInfo instead of creating its own.
//  2024-12-09: DirectX12: Let user specifies the DepthStencilView format by setting ImGui_ImplDX12_InitInfo::DSVFormat.
//  2024-11-15: DirectX12: *BREAKING CHANGE* Changed ImGui_ImplDX12_Init() signature to take a ImGui_ImplDX12_InitInfo struct. Legacy ImGui_ImplDX12_Init() signature is still supported (will obsolete).
//  2024-11-15: DirectX12: *BREAKING CHANGE* User is now required to pass function pointers to allocate/free SRV Descriptors. We provide convenience legacy fields to pass a single descriptor, matching the old API, but upcoming features will want multiple.
//  2024-10-23: DirectX12: Unmap() call specify written range. The range is informational and may be used by debug tools.
//  2024-10-07: DirectX12: Changed default texture sampler to Clamp instead of Repeat/Wrap.
//  2024-10-07: DirectX12: Expose selected render state in ImGui_ImplDX12_RenderState, which you can access in 'void* platform_io.Renderer_RenderState' during draw callbacks.
//  2024-10-07: DirectX12: Compiling with '#define ImTextureID=ImU64' is unnecessary now that dear imgui defaults ImTextureID to u64 instead of void*.
//  2022-10-11: Using 'nullptr' instead of 'NULL' as per our switch to C++11.
//  2021-06-29: Reorganized backend to pull data from a single structure to facilitate usage with multiple-contexts (all g_XXXX access changed to bd->XXXX).
//  2021-05-19: DirectX12: Replaced direct access to ImDrawCmd::TextureId with a call to ImDrawCmd::GetTexID(). (will become a requirement)
//  2021-02-18: DirectX12: Change blending equation to preserve alpha in output buffer.
//  2021-01-11: DirectX12: Improve Windows 7 compatibility (for D3D12On7) by loading d3d12.dll dynamically.
//  2020-09-16: DirectX12: Avoid rendering calls with zero-sized scissor rectangle since it generates a validation layer warning.
//  2020-09-08: DirectX12: Clarified support for building on 32-bit systems by redefining ImTextureID.
//  2019-10-18: DirectX12: *BREAKING CHANGE* Added extra ID3D12DescriptorHeap parameter to ImGui_ImplDX12_Init() function.
//  2019-05-29: DirectX12: Added support for large mesh (64K+ vertices), enable ImGuiBackendFlags_RendererHasVtxOffset flag.
//  2019-04-30: DirectX12: Added support for special ImDrawCallback_ResetRenderState callback to reset render state.
//  2019-03-29: Misc: Various minor tidying up.
//  2018-12-03: Misc: Added #pragma comment statement to automatically link with d3dcompiler.lib when using D3DCompile().
//  2018-11-30: Misc: Setting up io.BackendRendererName so it can be displayed in the About Window.
//  2018-06-12: DirectX12: Moved the ID3D12GraphicsCommandList* parameter from NewFrame() to RenderDrawData().
//  2018-06-08: Misc: Extracted imgui_impl_dx12.cpp/.h away from the old combined DX12+Win32 example.
//  2018-06-08: DirectX12: Use draw_data->DisplayPos and draw_data->DisplaySize to setup projection matrix and clipping rectangle (to ease support for future multi-viewport).
//  2018-02-22: Merged into master with all Win32 code synchronized to other examples.

#ifndef IMGUI_DISABLE
#include "imgui_impl_wisdom.h"

#if __has_include(<wisdom/wisdom_extended_allocation.hpp>)
#include <wisdom/wisdom_extended_allocation.hpp>
#define WISDOM_EXTENDED_ALLOCATION_AVAILABLE
#endif

// Shader codes for the default embedded shaders
// Pixel Shader
//                          "struct PS_INPUT\
//{\
//  float4 pos : SV_POSITION;\
//  float4 col : COLOR0;\
//  float2 uv  : TEXCOORD0;\
//};\
//SamplerState sampler0 : register(s0);\
//Texture2D texture0 : register(t0);\
//\
//float4 main(PS_INPUT input) : SV_Target\
//{\
//  float4 out_col = input.col * texture0.Sample(sampler0, input.uv); \
//  return out_col; \
//}";

// Vertex Shader
//                          "cbuffer vertexBuffer : register(b0) \
//{\
//  float4x4 ProjectionMatrix; \
//};\
//struct VS_INPUT\
//{\
//  float2 pos : POSITION;\
//  float4 col : COLOR0;\
//  float2 uv  : TEXCOORD0;\
//};\
//\
//struct PS_INPUT\
//{\
//  float4 pos : SV_POSITION;\
//  float4 col : COLOR0;\
//  float2 uv  : TEXCOORD0;\
//};\
//\
//PS_INPUT main(VS_INPUT input)\
//{\
//  PS_INPUT output;\
//  output.pos = mul( ProjectionMatrix, float4(input.pos.xy, 0.f, 1.f));\
//  output.col = input.col;\
//  output.uv  = input.uv;\
//  return output;\
//}";

struct ImGui_ImplWisdom_RenderBuffers {
    wis::Buffer vertex_buffer;
    wis::Buffer index_buffer;
    uint32_t vertex_count;
    uint32_t index_count;
};

struct ImGui_ImplWisdom_ConstantBuffer {
    float mvp[4][4];
};

// POD
struct ImGui_ImplWisdom_Data {
    wis::Device* device;
    wis::ResourceAllocator* allocator;
    wis::CommandQueue* command_queue;
    wis::DescriptorStorage* desc_storage;

#if defined(WISDOM_EXTENDED_ALLOCATION_AVAILABLE)
    wis::ExtendedAllocation* extended_allocation;
#endif

    wis::RootSignature root_signature;
    wis::PipelineState pipeline_state;
    wis::Texture font_texture;
    wis::ShaderResource font_texture_srv;
    wis::Sampler font_texture_sampler;

    wis::DataFormat rtv_format;
    wis::DataFormat dsv_format;

    uint32_t frame_index;
    uint32_t frames_in_flight_count;
    uint32_t srv_descriptor_offset;
    uint32_t srv_descriptor_binding;
    uint32_t sampler_descriptor_offset;
    uint32_t sampler_descriptor_binding;
    wis::QueueType queue_type;

    // ImGui_ImplDX12_Texture FontTexture;
    // bool LegacySingleDescriptorUsed;

    ImGui_ImplWisdom_RenderBuffers frame_resources[0]; // flexible array member
};

static const ImGui_ImplWisdom_DescriptorRequirement requirements[] = {
    { wis::DescriptorType::Texture, 1 },
    { wis::DescriptorType::Sampler, 1 }
};

static ImGui_ImplWisdom_Data* ImGui_ImplWisdom_GetBackendData()
{
    return ImGui::GetCurrentContext()
            ? reinterpret_cast<ImGui_ImplWisdom_Data*>(ImGui::GetIO().BackendRendererUserData)
            : nullptr;
}

static wis::Result ImGui_ImplWisdom_ResizeRenderBuffers(ImGui_ImplWisdom_RenderBuffers* buffers, const wis::ResourceAllocator& allocator, uint32_t vertex_count, uint32_t index_count)
{
    wis::Result result = wis::success;
    if (buffers->vertex_count < vertex_count) {
        buffers->vertex_buffer = allocator.CreateBuffer(result,
                                                        (vertex_count + 5000) * sizeof(ImDrawVert),
                                                        wis::BufferUsage::VertexBuffer,
                                                        wis::MemoryType::Upload,
                                                        wis::MemoryFlags::Mapped);
        buffers->vertex_count = vertex_count + 5000;
    }
    if (buffers->index_count < index_count) {
        buffers->index_buffer = allocator.CreateBuffer(result,
                                                       (index_count + 10000) * sizeof(ImDrawIdx),
                                                       wis::BufferUsage::IndexBuffer,
                                                       wis::MemoryType::Upload,
                                                       wis::MemoryFlags::Mapped);
        buffers->index_count = index_count + 10000;
    }
    return result;
}

static void ImGui_ImplWisdom_SetupRenderState(ImDrawData* draw_data, wis::CommandList& command_list, ImGui_ImplWisdom_RenderBuffers* fr)
{
    ImGui_ImplWisdom_Data* bd = ImGui_ImplWisdom_GetBackendData();

    // Setup orthographic projection matrix into our constant buffer
    // Our visible imgui space lies from draw_data->DisplayPos (top left) to draw_data->DisplayPos+data_data->DisplaySize (bottom right).
    float L = draw_data->DisplayPos.x;
    float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
    float T = draw_data->DisplayPos.y;
    float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;
    ImGui_ImplWisdom_ConstantBuffer vertex_constant_buffer{
        {
                { 2.0f / (R - L), 0.0f, 0.0f, 0.0f },
                { 0.0f, 2.0f / (T - B), 0.0f, 0.0f },
                { 0.0f, 0.0f, 0.5f, 0.0f },
                { (R + L) / (L - R), (T + B) / (B - T), 0.5f, 1.0f },
        }
    };

    // Setup viewport
    wis::Viewport vp{
        0.0f, 0.0f,
        draw_data->DisplaySize.x, draw_data->DisplaySize.y,
        0.0f, 1.0f
    };
    command_list.RSSetViewport(vp);

    // Bind shader and vertex buffers
    uint32_t offset = 0;
    wis::VertexBufferBinding vertex_buffer_binding{
        fr->vertex_buffer,
        fr->vertex_count * sizeof(ImDrawVert),
        sizeof(ImDrawVert),
        offset
    };
    command_list.IASetVertexBuffers(&vertex_buffer_binding, 1, 0);
    command_list.IASetIndexBuffer(fr->index_buffer,
                                  sizeof(ImDrawIdx) == 2 ? wis::IndexType::UInt16 : wis::IndexType::UInt32,
                                  0);
    command_list.IASetPrimitiveTopology(wis::PrimitiveTopology::TriangleList);
    command_list.SetPipelineState(bd->pipeline_state);
    command_list.SetRootSignature(bd->root_signature);
    command_list.SetDescriptorStorage(bd->desc_storage);
    command_list.SetPushConstants(&vertex_constant_buffer, 16, 0, wis::ShaderStages::Vertex);
}

static wis::Result ImGui_ImplWisdom_CreateFontsTexture()
{
    using namespace wis;
    wis::Result result = wis::success;

    // Build texture atlas
    ImGuiIO& io = ImGui::GetIO();
    ImGui_ImplWisdom_Data* bd = ImGui_ImplWisdom_GetBackendData();
    uint8_t* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    // Create texture
    wis::TextureDesc desc{
        wis::DataFormat::RGBA8Unorm,
        wis::Size3D{ static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1 },
        1,
        wis::TextureLayout::Texture2D,
        wis::SampleRate::S1,
        wis::TextureUsage::ShaderResource | wis::TextureUsage::HostCopy
    };
    bd->font_texture = bd->allocator->CreateTexture(result, desc);
    if (result.status != wis::Status::Ok) {
        return result;
    }

    // Upload texture to graphics system using a queue upload
    wis::Buffer staging_buffer = bd->allocator->CreateBuffer(result,
                                                             width * height * 4,
                                                             wis::BufferUsage::CopySrc,
                                                             wis::MemoryType::Upload,
                                                             wis::MemoryFlags::Mapped);
    void* mapping = staging_buffer.MapRaw();
    if (!mapping) {
        return wis::Result{ wis::Status::Error, "Failed to map staging buffer for font texture upload" };
    }

    memcpy(mapping, pixels, width * height * 4);
    staging_buffer.Unmap();

    wis::CommandList command_list = bd->device->CreateCommandList(result, bd->queue_type);
    if (result.status != wis::Status::Ok) {
        return result;
    }

    wis::Fence fence = bd->device->CreateFence(result);
    if (result.status != wis::Status::Ok) {
        return result;
    }

    wis::TextureBarrier barrier_in{
        wis::BarrierSync::None,
        wis::BarrierSync::Copy,
        wis::ResourceAccess::NoAccess,
        wis::ResourceAccess::CopyDest,
        wis::TextureState::Undefined,
        wis::TextureState::CopyDest
    };
    command_list.TextureBarrier(barrier_in, bd->font_texture);

    wis::BufferTextureCopyRegion region{
        0,
        { { 0, 0, 0 },
          { static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1 },
          0,
          0,
          wis::DataFormat::RGBA8Unorm }
    };
    command_list.CopyBufferToTexture(staging_buffer, bd->font_texture, &region, 1);

    wis::TextureBarrier barrier_out{
        wis::BarrierSync::Copy,
        wis::BarrierSync::PixelShading,
        wis::ResourceAccess::CopyDest,
        wis::ResourceAccess::ShaderResource,
        wis::TextureState::CopyDest,
        wis::TextureState::ShaderResource
    };
    command_list.TextureBarrier(barrier_out, bd->font_texture);
    if (!command_list.Close()) {
        return wis::Result{ wis::Status::Error, "Failed to close command list for font texture upload" };
    }

    wis::CommandListView command_lists[] = { command_list };
    bd->command_queue->ExecuteCommandLists(command_lists, 1);
    result = bd->command_queue->SignalQueue(fence, 1);
    if (result.status != wis::Status::Ok) {
        return result;
    }

    result = fence.Wait(1);
    if (result.status != wis::Status::Ok) {
        return result;
    }

    // Create shader resource view
    wis::ShaderResourceDesc srv_desc{
        wis::DataFormat::RGBA8Unorm,
        wis::TextureViewType::Texture2D,
        {},
        { 0, 1, 0, 1 }
    };
    bd->font_texture_srv = bd->device->CreateShaderResource(result, bd->font_texture, srv_desc);
    if (result.status != wis::Status::Ok) {
        return result;
    }

    wis::SamplerDesc sampler_desc{};
    sampler_desc.min_filter = wis::Filter::Linear;
    sampler_desc.mag_filter = wis::Filter::Linear;
    sampler_desc.mip_filter = wis::Filter::Linear;
    sampler_desc.anisotropic = false;
    sampler_desc.address_u = wis::AddressMode::ClampToBorder;
    sampler_desc.address_v = wis::AddressMode::ClampToBorder;
    sampler_desc.address_w = wis::AddressMode::ClampToBorder;
    sampler_desc.min_lod = 0.0f;
    sampler_desc.max_lod = 0.0f;
    sampler_desc.mip_lod_bias = 0.0f;
    sampler_desc.comparison_op = wis::Compare::Never;
    sampler_desc.border_color = { 0.0f, 0.0f, 0.0f, 0.0f };
    bd->font_texture_sampler = bd->device->CreateSampler(result, sampler_desc);

    bd->desc_storage->WriteTexture(bd->srv_descriptor_binding, bd->srv_descriptor_offset, bd->font_texture_srv);
    bd->desc_storage->WriteSampler(bd->sampler_descriptor_binding, bd->sampler_descriptor_offset, bd->font_texture_sampler);

    // Store our identifier
    io.Fonts->SetTexID(ImTextureID{ bd->srv_descriptor_offset });
    return result;
}

static wis::Result ImGui_ImplWisdom_CreateFontsTexture_DirectUpload()
{
    using namespace wis;
    wis::Result result = wis::success;

    // Build texture atlas
    ImGuiIO& io = ImGui::GetIO();
    ImGui_ImplWisdom_Data* bd = ImGui_ImplWisdom_GetBackendData();
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    // Create texture
    wis::TextureDesc desc{
        wis::DataFormat::RGBA8Unorm,
        wis::Size3D{ static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1 },
        1,
        wis::TextureLayout::Texture2D,
        wis::SampleRate::S1,
        wis::TextureUsage::ShaderResource | wis::TextureUsage::HostCopy
    };
    bd->font_texture = bd->extended_allocation->CreateGPUUploadTexture(result, *bd->allocator, desc);
    if (result.status != wis::Status::Ok) {
        return result;
    }

    // Upload texture to graphics system
    wis::TextureRegion region{
        { 0, 0, 0 },
        { static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1 },
        0,
        0,
        wis::DataFormat::RGBA8Unorm
    };
    result = bd->extended_allocation->WriteMemoryToSubresourceDirect(pixels,
                                                                     bd->font_texture,
                                                                     wis::TextureState::Common,
                                                                     region);

    if (result.status != wis::Status::Ok) {
        return result;
    }

    // Create shader resource view
    wis::ShaderResourceDesc srv_desc{
        wis::DataFormat::RGBA8Unorm,
        wis::TextureViewType::Texture2D,
        {},
        { 0, 1, 0, 1 }
    };
    bd->font_texture_srv = bd->device->CreateShaderResource(result, bd->font_texture, srv_desc);
    if (result.status != wis::Status::Ok) {
        return result;
    }

    wis::SamplerDesc sampler_desc{};
    sampler_desc.min_filter = wis::Filter::Linear;
    sampler_desc.mag_filter = wis::Filter::Linear;
    sampler_desc.mip_filter = wis::Filter::Linear;
    sampler_desc.anisotropic = false;
    sampler_desc.address_u = wis::AddressMode::ClampToBorder;
    sampler_desc.address_v = wis::AddressMode::ClampToBorder;
    sampler_desc.address_w = wis::AddressMode::ClampToBorder;
    sampler_desc.min_lod = 0.0f;
    sampler_desc.max_lod = 0.0f;
    sampler_desc.mip_lod_bias = 0.0f;
    sampler_desc.comparison_op = wis::Compare::Never;
    sampler_desc.border_color = { 0.0f, 0.0f, 0.0f, 0.0f };
    bd->font_texture_sampler = bd->device->CreateSampler(result, sampler_desc);

    bd->desc_storage->WriteTexture(bd->srv_descriptor_binding, bd->srv_descriptor_offset, bd->font_texture_srv);
    bd->desc_storage->WriteSampler(bd->sampler_descriptor_binding, bd->sampler_descriptor_offset, bd->font_texture_sampler);

    // Store our identifier
    io.Fonts->SetTexID(ImTextureID{ bd->srv_descriptor_offset });
    return result;
}

bool ImGui_ImplWisdom_CreateDeviceObjects(wis::ShaderView vs, wis::ShaderView ps, wis::DescriptorBindingDesc* desc_bindings, uint32_t bindings_count)
{
    wis::Result result = wis::success;
    ImGui_ImplWisdom_Data* bd = ImGui_ImplWisdom_GetBackendData();
    if (!bd || !bd->device)
        return false;

    // Create the root signature
    wis::PushConstant push_constants[] = {
        { wis::ShaderStages::Vertex, sizeof(ImGui_ImplWisdom_ConstantBuffer), 0 },
        { wis::ShaderStages::Pixel, sizeof(uint32_t) * 2, 0 }
    };
    bd->root_signature = bd->device->CreateRootSignature(result, push_constants, 2, nullptr, 0, desc_bindings, bindings_count);
    if (result.status != wis::Status::Ok) {
        return false;
    }

    wis::InputSlotDesc slots[] = {
        { 0, sizeof(ImDrawVert), wis::InputClass::PerVertex }
    };
    wis::InputAttribute attributes[] = {
        { 0, "POSITION", 0, 0, wis::DataFormat::RG32Float, (uint32_t)offsetof(ImDrawVert, pos) },
        { 0, "TEXCOORD", 0, 1, wis::DataFormat::RG32Float, (uint32_t)offsetof(ImDrawVert, uv) },
        { 0, "COLOR", 0, 2, wis::DataFormat::RGBA8Unorm, (uint32_t)offsetof(ImDrawVert, col) }
    };

    // Create shaders

    // Create the pipeline state
    wis::RasterizerDesc rasterizer_desc{};
    rasterizer_desc.cull_mode = wis::CullMode::None;

    wis::BlendStateDesc blend_desc{};
    blend_desc.attachments[0].blend_enable = true;
    blend_desc.attachments[0].src_color_blend = wis::BlendFactor::SrcAlpha;
    blend_desc.attachments[0].dst_color_blend = wis::BlendFactor::InvSrcAlpha;
    blend_desc.attachments[0].color_blend_op = wis::BlendOp::Add;
    blend_desc.attachments[0].src_alpha_blend = wis::BlendFactor::One;
    blend_desc.attachments[0].dst_alpha_blend = wis::BlendFactor::InvSrcAlpha;
    blend_desc.attachments[0].alpha_blend_op = wis::BlendOp::Add;
    blend_desc.attachments[0].color_write_mask = wis::ColorComponents::All;
    blend_desc.attachment_count = 1;

    wis::GraphicsPipelineDesc pipeline_state_desc{};
    pipeline_state_desc.root_signature = bd->root_signature;
    pipeline_state_desc.input_layout = {
        slots,
        1,
        attributes,
        3
    };
    pipeline_state_desc.shaders.vertex = vs;
    pipeline_state_desc.shaders.pixel = ps;
    pipeline_state_desc.attachments = {
        { bd->rtv_format },
        1,
        bd->dsv_format
    };
    pipeline_state_desc.rasterizer = &rasterizer_desc;
    pipeline_state_desc.topology_type = wis::TopologyType::Triangle;
    pipeline_state_desc.blend = &blend_desc;
    bd->pipeline_state = bd->device->CreateGraphicsPipeline(result, pipeline_state_desc);
    if (result.status != wis::Status::Ok) {
        return false;
    }

    // Create font texture
#if defined(WISDOM_EXTENDED_ALLOCATION_AVAILABLE)
    if (bd->extended_allocation && bd->extended_allocation->SupportedDirectGPUUpload(wis::DataFormat::RGBA8Unorm)) {
        result = ImGui_ImplWisdom_CreateFontsTexture_DirectUpload();
        if (result.status == wis::Status::Ok) {
            return true;
        }
    }
#endif

    result = ImGui_ImplWisdom_CreateFontsTexture();
    return result.status == wis::Status::Ok;
}

void ImGui_ImplWisdom_InvalidateDeviceObjects()
{
    ImGui_ImplWisdom_Data* bd = ImGui_ImplWisdom_GetBackendData();
    if (!bd || !bd->device)
        return;

    // We only need to release the buffers, the rest of the resources will be released by the allocator
    for (UINT i = 0; i < bd->frames_in_flight_count; i++) {
        ImGui_ImplWisdom_RenderBuffers* fr = &bd->frame_resources[i];
        fr->index_buffer = {};
        fr->vertex_buffer = {};
    }
    memset(bd->frame_resources, 0, sizeof(ImGui_ImplWisdom_RenderBuffers) * bd->frames_in_flight_count);
}

bool ImGui_ImplWisdom_Init(ImGui_ImplWisdom_InitInfo* init_info)
{
    ImGuiIO& io = ImGui::GetIO();
    IMGUI_CHECKVERSION();
    IM_ASSERT(io.BackendRendererUserData == nullptr && "Already initialized a renderer backend!");

    // Setup backend capabilities flags
    size_t size_bytes = sizeof(ImGui_ImplWisdom_Data) + sizeof(ImGui_ImplWisdom_RenderBuffers) * init_info->frames_in_flight_count;
    ImGui_ImplWisdom_Data* bd = reinterpret_cast<ImGui_ImplWisdom_Data*>(ImGui::MemAlloc(size_bytes));
    memset(bd, 0, size_bytes);

    bd->device = init_info->device;
    bd->allocator = init_info->allocator;
    bd->command_queue = init_info->command_queue;
    bd->desc_storage = init_info->desc_storage;

    bd->rtv_format = init_info->rtv_format;
    bd->dsv_format = init_info->dsv_format;

    bd->frames_in_flight_count = init_info->frames_in_flight_count;
    bd->srv_descriptor_offset = init_info->srv_descriptor_offset;
    bd->srv_descriptor_binding = init_info->srv_descriptor_binding;
    bd->sampler_descriptor_offset = init_info->sampler_descriptor_offset;
    bd->sampler_descriptor_binding = init_info->sampler_descriptor_binding;
    bd->queue_type = init_info->queue_type;

    io.BackendRendererUserData = static_cast<void*>(bd);
    io.BackendRendererName = "imgui_impl_wisdom";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset; // We can honor the ImDrawCmd::VtxOffset field, allowing for large meshes.

    ImGui_ImplWisdom_ExtensionStruct* ext = reinterpret_cast<ImGui_ImplWisdom_ExtensionStruct*>(init_info->extensions);
    while (ext) {
        switch (ext->type) {
#if defined(WISDOM_EXTENDED_ALLOCATION_AVAILABLE)
        case ImGui_ImplWisdom_ExtensionStructType_ExtendedAllocation: {
            ImGui_ExtensionStruct_ExtendedAllocation* ext_alloc = reinterpret_cast<ImGui_ExtensionStruct_ExtendedAllocation*>(ext);
            bd->extended_allocation = static_cast<wis::ExtendedAllocation*>(ext_alloc->extended_allocation);
        } break;
#endif
        default:
            break;
        }
        ext = ext->next;
    }

    // Create buffers with a default size (they will later be grown as needed)
    for (uint32_t i = 0; i < init_info->frames_in_flight_count; i++) {
        ImGui_ImplWisdom_RenderBuffers* fr = &bd->frame_resources[i];
        ImGui_ImplWisdom_ResizeRenderBuffers(fr, *bd->allocator, 1, 1);
    }

    return ImGui_ImplWisdom_CreateDeviceObjects(init_info->vertex_shader, init_info->pixel_shader, init_info->descriptor_bindings, init_info->descriptor_bindings_count);
}

void ImGui_ImplWisdom_Shutdown()
{
    ImGui_ImplWisdom_Data* bd = ImGui_ImplWisdom_GetBackendData();
    IM_ASSERT(bd != nullptr && "No renderer backend to shutdown, or already shutdown?");
    ImGuiIO& io = ImGui::GetIO();

    ImGui_ImplWisdom_InvalidateDeviceObjects();

    io.BackendRendererName = nullptr;
    io.BackendRendererUserData = nullptr;
    io.BackendFlags &= ~ImGuiBackendFlags_RendererHasVtxOffset;

    bd->~ImGui_ImplWisdom_Data();
    ImGui::MemFree(bd);
}

void ImGui_ImplWisdom_NewFrame()
{
    ImGui_ImplWisdom_Data* bd = ImGui_ImplWisdom_GetBackendData();
    IM_ASSERT(bd != nullptr && "Context or backend not initialized! Did you call ImGui_ImplWisdom_Init()?");

    bd->frame_index++;
}

ImGui_ImplWisdom_DescriptorRequirement* ImGui_ImplWisdom_GetDescriptorRequirements(uint32_t* requirements_count)
{
    *requirements_count = sizeof(requirements) / sizeof(ImGui_ImplWisdom_DescriptorRequirement);
    return const_cast<ImGui_ImplWisdom_DescriptorRequirement*>(requirements);
}

void ImGui_ImplWisdom_RenderDrawData(ImDrawData* draw_data, wis::CommandList& command_list)
{
    // Avoid rendering when minimized
    if (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f)
        return;

    ImGui_ImplWisdom_Data* bd = ImGui_ImplWisdom_GetBackendData();
    ImGui_ImplWisdom_RenderBuffers* fr = &bd->frame_resources[bd->frame_index % bd->frames_in_flight_count];

    // Create and grow vertex/index buffers if needed
    ImGui_ImplWisdom_ResizeRenderBuffers(fr, *bd->allocator, draw_data->TotalVtxCount, draw_data->TotalIdxCount);

    // Upload vertex/index data into a single contiguous GPU buffer
    ImDrawVert* vtx_resource = fr->vertex_buffer.Map<ImDrawVert>();
    if (!vtx_resource)
        return;

    ImDrawIdx* idx_resource = fr->index_buffer.Map<ImDrawIdx>();
    if (!idx_resource)
        return;

    for (int n = 0; n < draw_data->CmdListsCount; n++) {
        const ImDrawList* draw_list = draw_data->CmdLists[n];
        memcpy(vtx_resource, draw_list->VtxBuffer.Data, draw_list->VtxBuffer.Size * sizeof(ImDrawVert));
        memcpy(idx_resource, draw_list->IdxBuffer.Data, draw_list->IdxBuffer.Size * sizeof(ImDrawIdx));
        vtx_resource += draw_list->VtxBuffer.Size;
        idx_resource += draw_list->IdxBuffer.Size;
    }

    fr->vertex_buffer.Unmap();
    fr->index_buffer.Unmap();

    // Setup desired DX state
    ImGui_ImplWisdom_SetupRenderState(draw_data, command_list, fr);

    // Setup render state structure (for callbacks and custom texture bindings)
    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
    // ImGui_ImplWisdom_RenderState render_state;
    // render_state.Device = bd->pd3dDevice;
    // render_state.CommandList = command_list;
    // platform_io.Renderer_RenderState = &render_state;

    // Render command lists
    // (Because we merged all buffers into a single one, we maintain our own offset into them)
    int global_vtx_offset = 0;
    int global_idx_offset = 0;
    const uint32_t srv_descriptor_offset = bd->srv_descriptor_offset;
    ImVec2 clip_off = draw_data->DisplayPos;
    for (int n = 0; n < draw_data->CmdListsCount; n++) {
        const ImDrawList* draw_list = draw_data->CmdLists[n];
        for (int cmd_i = 0; cmd_i < draw_list->CmdBuffer.Size; cmd_i++) {
            const ImDrawCmd* pcmd = &draw_list->CmdBuffer[cmd_i];
            if (pcmd->UserCallback != nullptr) {
                // User callback, registered via ImDrawList::AddCallback()
                // (ImDrawCallback_ResetRenderState is a special callback value used by the user to request the renderer to reset render state.)
                if (pcmd->UserCallback == ImDrawCallback_ResetRenderState)
                    ImGui_ImplWisdom_SetupRenderState(draw_data, command_list, fr);
                else
                    pcmd->UserCallback(draw_list, pcmd);
            } else {
                // Project scissor/clipping rectangles into framebuffer space
                ImVec2 clip_min(pcmd->ClipRect.x - clip_off.x, pcmd->ClipRect.y - clip_off.y);
                ImVec2 clip_max(pcmd->ClipRect.z - clip_off.x, pcmd->ClipRect.w - clip_off.y);
                if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
                    continue;

                // Apply scissor/clipping rectangle
                const wis::Scissor r{ (LONG)clip_min.x, (LONG)clip_min.y, (LONG)clip_max.x, (LONG)clip_max.y };
                command_list.RSSetScissor(r);

                // Bind texture, Draw
                ImTextureID tex_id = pcmd->GetTexID() + srv_descriptor_offset;
                command_list.SetPushConstants(&tex_id, 1, 0, wis::ShaderStages::Pixel);
                command_list.DrawIndexedInstanced(pcmd->ElemCount, 1, pcmd->IdxOffset + global_idx_offset, pcmd->VtxOffset + global_vtx_offset, 0);
            }
        }
        global_idx_offset += draw_list->IdxBuffer.Size;
        global_vtx_offset += draw_list->VtxBuffer.Size;
    }
    platform_io.Renderer_RenderState = nullptr;
}

//-----------------------------------------------------------------------------

#endif // #ifndef IMGUI_DISABLE
