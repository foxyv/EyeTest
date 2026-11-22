// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 foxyv
#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// Every user-adjustable camera setting. Ranges match PS3EYEDriver.
struct CameraSettings
{
    int gain = 20;          // 0..63
    int exposure = 255;     // 0..255
    int sharpness = 0;      // 0..63
    int contrast = 0;       // 0..255
    int brightness = 20;    // 0..255
    int hue = 64;           // 0..128
    int saturation = 0;     // 0..255
    int red_balance = 128;  // 0..255
    int green_balance = 128;
    int blue_balance = 128;
    bool auto_gain = false;
    bool awb = true;
    bool flip_h = false;
    bool flip_v = false;
    bool test_pattern = false;

    bool operator==(const CameraSettings&) const;
    bool operator!=(const CameraSettings& o) const { return !(*this == o); }
};

struct CameraMode
{
    bool vga = true;  // 640x480 when true, else 320x240
    int fps = 60;
    bool operator==(const CameraMode& o) const { return vga == o.vga && fps == o.fps; }
};

enum class CameraState { Searching, Streaming, InUse, Error };

// Owns the camera on a background thread. The UI thread only sets the
// desired settings/mode and reads the latest frame; all driver calls happen
// on the worker thread, so nothing touches the device from two threads.
class CameraWorker
{
public:
    CameraWorker();
    ~CameraWorker();

    void set_settings(const CameraSettings& s);
    void set_mode(const CameraMode& m);
    void set_camera_index(int index);

    // Copies the newest frame (BGR24) if one arrived since `last_seq`.
    bool latest_frame(std::vector<uint8_t>& out, int& w, int& h, uint64_t& last_seq);

    CameraState state() const { return state_.load(); }
    std::string status() const;
    int camera_count() const { return camera_count_.load(); }
    int actual_fps() const { return actual_fps_.load(); }
    double measured_fps() const { return measured_fps_.load(); }
    uint64_t dropped_frames() const { return dropped_.load(); }

    static const std::vector<int>& framerates(bool vga);

private:
    void run();
    void set_status(CameraState st, const std::string& msg);

    std::thread thread_;
    std::atomic<bool> quit_ { false };

    std::mutex desired_mutex_;
    CameraSettings desired_settings_;
    CameraMode desired_mode_;
    int desired_index_ = 0;
    uint64_t desired_gen_ = 1;

    std::mutex frame_mutex_;
    std::vector<uint8_t> frame_;
    int frame_w_ = 0, frame_h_ = 0;
    uint64_t frame_seq_ = 0;

    mutable std::mutex status_mutex_;
    std::string status_ = "Looking for a camera...";
    std::atomic<CameraState> state_ { CameraState::Searching };
    std::atomic<int> camera_count_ { 0 };
    std::atomic<int> actual_fps_ { 0 };
    std::atomic<double> measured_fps_ { 0 };
    std::atomic<uint64_t> dropped_ { 0 };
};
