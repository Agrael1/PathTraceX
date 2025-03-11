#pragma once
#include <wisdom/wisdom.hpp>
#include <filesystem>
#include <fstream>

namespace w {
static constexpr wis::DataFormat swap_format = wis::DataFormat::RGBA8Unorm; // standard format for the application
static constexpr wis::DataFormat depth_format = wis::DataFormat::D32Float; // standard format for the application
static constexpr uint32_t swap_frames = 2; //
static constexpr uint32_t flight_frames = 2; //

struct Exception : public std::exception {
    Exception(std::string message)
        : message(std::move(message))
    {
    }

    const char* what() const noexcept override
    {
        return message.c_str();
    }

    std::string message;
};

inline void CheckResult(wis::Result res)
{
    if (res.status != wis::Status::Ok) {
        throw Exception(res.error);
    }
}
inline std::string LoadShader(std::filesystem::path p)
{
    if constexpr (wis::shader_intermediate == wis::ShaderIntermediate::DXIL) {
        p += u".cso";
    } else {
        p += u".spv";
    }

    if (!std::filesystem::exists(p)) {
        throw w::Exception(wis::format("Shader file not found: {}", p.string()));
    }

    std::ifstream t{ p, std::ios::binary };
    t.seekg(0, std::ios::end);
    size_t size = t.tellg();
    std::string ret;
    ret.resize(size);
    t.seekg(0);
    t.read(ret.data(), size);
    return ret;
}
} // namespace w