#pragma once

#include "camera_worker.hpp"

#include <filesystem>
#include <string>
#include <vector>

// Presets are plain "key=value" text files in a "presets" folder next to the exe.
namespace presets {

std::filesystem::path folder();
std::vector<std::string> list();
bool save(const std::string& name, const CameraSettings& s, const CameraMode& m);
bool load(const std::string& name, CameraSettings& s, CameraMode& m);

} // namespace presets
