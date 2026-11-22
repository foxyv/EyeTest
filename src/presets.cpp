// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 foxyv
#include "presets.hpp"

#include <algorithm>
#include <fstream>
#include <map>
#include <windows.h>

namespace fs = std::filesystem;

namespace presets {

fs::path folder()
{
    wchar_t buf[MAX_PATH];
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    return fs::path(buf).parent_path() / "presets";
}

std::vector<std::string> list()
{
    std::vector<std::string> out;
    std::error_code ec;
    for (auto& e : fs::directory_iterator(folder(), ec))
        if (e.path().extension() == ".ini")
            out.push_back(e.path().stem().string());
    std::sort(out.begin(), out.end());
    return out;
}

static fs::path file_for(const std::string& name)
{
    std::string clean;
    for (char c : name)
        if (isalnum((unsigned char)c) || c == ' ' || c == '-' || c == '_') clean += c;
    if (clean.empty()) clean = "preset";
    return folder() / (clean + ".ini");
}

#define FIELDS(X) X(gain) X(exposure) X(sharpness) X(contrast) X(brightness) X(hue) \
    X(saturation) X(red_balance) X(green_balance) X(blue_balance) X(auto_gain) X(awb) \
    X(flip_h) X(flip_v) X(test_pattern)

bool save(const std::string& name, const CameraSettings& s, const CameraMode& m)
{
    std::error_code ec;
    fs::create_directories(folder(), ec);
    std::ofstream f(file_for(name));
    if (!f) return false;
#define W(k) f << #k "=" << (int)s.k << "\n";
    FIELDS(W)
#undef W
    f << "vga=" << (int)m.vga << "\nfps=" << m.fps << "\n";
    return (bool)f;
}

bool load(const std::string& name, CameraSettings& s, CameraMode& m)
{
    std::ifstream f(file_for(name));
    if (!f) return false;
    std::map<std::string, int> kv;
    std::string line;
    while (std::getline(f, line))
    {
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        try { kv[line.substr(0, eq)] = std::stoi(line.substr(eq + 1)); } catch (...) {}
    }
    CameraSettings d;
#define R(k) if (kv.count(#k)) s.k = (decltype(d.k))kv[#k];
    FIELDS(R)
#undef R
    if (kv.count("vga")) m.vga = kv["vga"] != 0;
    if (kv.count("fps")) m.fps = kv["fps"];
    return true;
}

} // namespace presets
