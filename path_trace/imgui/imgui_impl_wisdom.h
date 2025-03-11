#pragma once
#include "imgui.h" // IMGUI_IMPL_API
#ifndef IMGUI_DISABLE
#include <wisdom/wisdom.hpp>

enum ImGui_ImplWisdom_ExtensionStructType {
    ImGui_ImplWisdom_ExtensionStructType_ExtendedAllocation,
};

struct ImGui_ImplWisdom_ExtensionStruct {
    ImGui_ImplWisdom_ExtensionStructType type;
    ImGui_ImplWisdom_ExtensionStruct* next;
};

struct ImGui_ExtensionStruct_ExtendedAllocation {
    ImGui_ImplWisdom_ExtensionStructType type;
    void* next;
    wis::DeviceExtension* extended_allocation;
};

struct ImGui_ImplWisdom_InitInfo {
    void* extensions;

    wis::Device* device;
    wis::ResourceAllocator* allocator;
    wis::CommandQueue* command_queue;
    wis::DescriptorStorage* desc_storage;

    wis::ShaderView vertex_shader;
    wis::ShaderView pixel_shader;

    wis::DescriptorBindingDesc* descriptor_bindings;
    uint32_t descriptor_bindings_count;

    wis::DataFormat rtv_format;
    wis::DataFormat dsv_format;

    uint32_t srv_descriptor_offset;
    uint32_t srv_descriptor_binding;
    uint32_t sampler_descriptor_offset;
    uint32_t sampler_descriptor_binding;
    uint32_t frames_in_flight_count;
    wis::QueueType queue_type;
};

struct ImGui_ImplWisdom_DescriptorRequirement {
    wis::DescriptorType type;
    uint32_t count;
};

bool ImGui_ImplWisdom_Init(ImGui_ImplWisdom_InitInfo* init_info);
void ImGui_ImplWisdom_Shutdown();
void ImGui_ImplWisdom_NewFrame();

ImGui_ImplWisdom_DescriptorRequirement* ImGui_ImplWisdom_GetDescriptorRequirements(uint32_t* requirements_count);

void ImGui_ImplWisdom_RenderDrawData(ImDrawData* draw_data, wis::CommandList& command_list);
#endif // #ifndef IMGUI_DISABLE