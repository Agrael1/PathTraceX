#include "scene.h"
#include "graphics.h"
#include <imgui.h>

w::Scene::Scene(Graphics& gfx, wis::Result result)
    : object_cbuffer(gfx.allocator.CreateUploadBuffer(result, sizeof(ObjectCBuffer) * spheres_count + 1))
    , sphere_static(gfx)
    , box_static(gfx)
    , mapped_cbuffer(object_cbuffer.Map<ObjectCBuffer>(), spheres_count + 1)
{
    object_views[0] = ObjectView("Box", {
                                                .diffuse = { 0.8f, 0.8f, 0.8f, 1.0f },
                                                .emissive = {},
                                                .roughness = 1,
                                        });

    constexpr DirectX::XMFLOAT4A sphere_colors[spheres_count] = {
        { 1.0f, 0.0f, 0.0f, 1.0f },
        { 0.0f, 1.0f, 0.0f, 1.0f },
        { 0.0f, 0.0f, 1.0f, 1.0f },
        { 1.0f, 1.0f, 0.0f, 1.0f },
    };

    for (int i = 1; i < objects_count; ++i) {
        object_views[i] = ObjectView(wis::format("Sphere {}", i), {
                                                                          .diffuse = sphere_colors[i],
                                                                          .emissive = {},
                                                                          .roughness = 1,
                                                                  });
    }
}

w::Scene::~Scene()
{
    object_cbuffer.Unmap();
}

void w::Scene::RenderUI()
{
    ImGui::Begin("Settings", nullptr);
    ImGui::PushItemWidth(150);
    // std::string fps_cpu_string = "FPS (CPU): " + std::to_string(fps_cpu);
    // ImGui::Text(fps_cpu_string.c_str());
    // std::string fps_string = "FPS (GPU): " + std::to_string(fps_cpu);
    // ImGui::Text(fps_cpu_string.c_str());
    // std::string iterations_string = "Iterations: " + std::to_string(iterations);
    // ImGui::Text(iterations_string.c_str());
    ImGui::Checkbox("Show Material Box", &show_material_window[0]);
    ImGui::Checkbox("Show Material Sph#1", &show_material_window[1]);
    ImGui::Checkbox("Show Material Sph#2", &show_material_window[2]);
    ImGui::Checkbox("Show Material Sph#3", &show_material_window[3]);
    ImGui::Checkbox("Show Material Sph#4", &show_material_window[4]);

    // ImGui::Checkbox("Accumulate", &accumulate);
    ImGui::Checkbox("Gama Correction", &gamma_correction);
    // if (ImGui::Checkbox("Limit Iterations", &limit_max_iterations)) {
    //     clear();
    // }
    // if (ImGui::SliderInt("Max Iterations", &max_iterations, 1, 1000)) {
    //     clear();
    // }

    // if (ImGui::Combo("Sampling", &current_sampling, SAMPLING_LABELS, IM_ARRAYSIZE(SAMPLING_LABELS))) {
    //     clear();
    // }
    // if (ImGui::Combo("BRDF", &current_brdf, BRDF_LABELS, IM_ARRAYSIZE(BRDF_LABELS))) {
    //     clear();
    // }
    // if (ImGui::SliderInt("Bounces", &bounces, 1, 100)) {
    //     clear();
    // }
    ImGui::End();

    for (int m = 0; m < objects_count; ++m) {
        if (show_material_window[m]) {
            object_views[m].RenderMatrialUI(mapped_cbuffer[m]);
        }
    }
}
