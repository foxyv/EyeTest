// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 foxyv
#include "camera_worker.hpp"

#include <ps3eye.hpp>
#include <libusb.h>

#include <chrono>

using namespace std::chrono_literals;
using clock_type = std::chrono::steady_clock;

bool CameraSettings::operator==(const CameraSettings& o) const
{
    return gain == o.gain && exposure == o.exposure && sharpness == o.sharpness &&
           contrast == o.contrast && brightness == o.brightness && hue == o.hue &&
           saturation == o.saturation && red_balance == o.red_balance &&
           green_balance == o.green_balance && blue_balance == o.blue_balance &&
           auto_gain == o.auto_gain && awb == o.awb && flip_h == o.flip_h &&
           flip_v == o.flip_v && test_pattern == o.test_pattern;
}

const std::vector<int>& CameraWorker::framerates(bool vga)
{
    static const std::vector<int> qvga { 30, 37, 40, 50, 60, 75, 90, 100, 125, 137, 150, 187, 205 };
    static const std::vector<int> vga_ { 15, 30, 40, 50, 60, 75, 83 };
    return vga ? vga_ : qvga;
}

// Counts PS3 Eye cameras Windows sees, whether or not we can open them.
// If this is >0 but the driver lists none, another program has the camera
// or it isn't on WinUSB.
static int count_attached_cameras()
{
    libusb_context* ctx = nullptr;
    if (libusb_init(&ctx) != 0) return 0;
    libusb_device** list = nullptr;
    ssize_t n = libusb_get_device_list(ctx, &list);
    int count = 0;
    for (ssize_t i = 0; i < n; i++)
    {
        libusb_device_descriptor d {};
        if (libusb_get_device_descriptor(list[i], &d) == 0 && d.idVendor == 0x1415 && d.idProduct == 0x2000)
            count++;
    }
    if (list) libusb_free_device_list(list, 1);
    libusb_exit(ctx);
    return count;
}

static void apply(ps3eye::camera& cam, const CameraSettings& s, const CameraSettings* prev)
{
#define CHANGED(f) (!prev || prev->f != s.f)
    if (CHANGED(auto_gain)) cam.set_auto_gain(s.auto_gain);
    if (CHANGED(awb)) cam.set_awb(s.awb);
    if (CHANGED(gain)) cam.set_gain(s.gain);
    if (CHANGED(exposure)) cam.set_exposure(s.exposure);
    if (CHANGED(sharpness)) cam.set_sharpness(s.sharpness);
    if (CHANGED(contrast)) cam.set_contrast(s.contrast);
    if (CHANGED(brightness)) cam.set_brightness(s.brightness);
    if (CHANGED(hue)) cam.set_hue(s.hue);
    if (CHANGED(saturation)) cam.set_saturation(s.saturation);
    if (CHANGED(red_balance)) cam.set_red_balance(s.red_balance);
    if (CHANGED(green_balance)) cam.set_green_balance(s.green_balance);
    if (CHANGED(blue_balance)) cam.set_blue_balance(s.blue_balance);
    if (CHANGED(flip_h) || CHANGED(flip_v)) cam.set_flip_status(s.flip_h, s.flip_v);
    if (CHANGED(test_pattern)) cam.set_test_pattern_status(s.test_pattern);
#undef CHANGED
}

CameraWorker::CameraWorker() : thread_(&CameraWorker::run, this) {}

CameraWorker::~CameraWorker()
{
    quit_ = true;
    if (thread_.joinable()) thread_.join();
}

void CameraWorker::set_settings(const CameraSettings& s)
{
    std::lock_guard l(desired_mutex_);
    desired_settings_ = s;
}

void CameraWorker::set_mode(const CameraMode& m)
{
    std::lock_guard l(desired_mutex_);
    desired_mode_ = m;
}

void CameraWorker::set_camera_index(int index)
{
    std::lock_guard l(desired_mutex_);
    if (desired_index_ != index) { desired_index_ = index; desired_gen_++; }
}

bool CameraWorker::latest_frame(std::vector<uint8_t>& out, int& w, int& h, uint64_t& last_seq)
{
    std::lock_guard l(frame_mutex_);
    if (frame_seq_ == last_seq || frame_.empty()) return false;
    out = frame_;
    w = frame_w_;
    h = frame_h_;
    last_seq = frame_seq_;
    return true;
}

std::string CameraWorker::status() const
{
    std::lock_guard l(status_mutex_);
    return status_;
}

void CameraWorker::set_status(CameraState st, const std::string& msg)
{
    state_ = st;
    std::lock_guard l(status_mutex_);
    status_ = msg;
}

void CameraWorker::run()
{
    std::shared_ptr<ps3eye::camera> cam;
    CameraSettings applied;
    CameraMode mode;
    uint64_t gen = 0;
    std::vector<uint8_t> buf;
    int fails = 0;

    auto close = [&] {
        if (cam) cam->stop();
        cam.reset();
    };

    while (!quit_)
    {
        CameraSettings want;
        CameraMode want_mode;
        int index;
        uint64_t want_gen;
        {
            std::lock_guard l(desired_mutex_);
            want = desired_settings_;
            want_mode = desired_mode_;
            index = desired_index_;
            want_gen = desired_gen_;
        }

        if (cam && want_gen != gen)
            close();

        // (Re)open the camera
        if (!cam)
        {
            auto list = ps3eye::list_devices();
            camera_count_ = (int)list.size();
            if (list.empty())
            {
                if (count_attached_cameras() > 0)
                    set_status(CameraState::InUse,
                               "Camera found but can't be opened. Close OpenTrack or any other program using it, "
                               "and check the camera uses the WinUSB driver.");
                else
                    set_status(CameraState::Searching, "No PS3 Eye found. Plug the camera in.");
                std::this_thread::sleep_for(1s);
                continue;
            }
            if (index >= (int)list.size()) index = 0;
            cam = list[index];
            gen = want_gen;
            mode = want_mode;
            if (!cam->init(mode.vga ? ps3eye::res_VGA : ps3eye::res_QVGA, mode.fps, ps3eye::fmt_BGR) ||
                (apply(*cam, want, nullptr), !cam->start()))
            {
                set_status(CameraState::Error, std::string("Couldn't start the camera: ") + cam->error_string());
                close();
                std::this_thread::sleep_for(1s);
                continue;
            }
            applied = want;
            actual_fps_ = cam->framerate();
            dropped_ = 0;
            fails = 0;
            set_status(CameraState::Streaming, "Streaming");
        }

        // Resolution or frame rate changed: restart the stream
        if (!(want_mode == mode))
        {
            mode = want_mode;
            cam->stop();
            if (!cam->init(mode.vga ? ps3eye::res_VGA : ps3eye::res_QVGA, mode.fps, ps3eye::fmt_BGR) ||
                (apply(*cam, want, nullptr), !cam->start()))
            {
                set_status(CameraState::Error, std::string("Couldn't change mode: ") + cam->error_string());
                close();
                continue;
            }
            applied = want;
            actual_fps_ = cam->framerate();
        }

        if (want != applied)
        {
            apply(*cam, want, &applied);
            applied = want;
        }

        // Grab a frame (blocks up to 1 s)
        int w = cam->width(), h = cam->height();
        buf.resize(size_t(w) * h * 3);
        if (cam->get_frame(buf.data()))
        {
            fails = 0;
            static auto t0 = clock_type::now();
            static int n = 0;
            n++;
            auto dt = std::chrono::duration<double>(clock_type::now() - t0).count();
            if (dt >= 0.5) { measured_fps_ = n / dt; n = 0; t0 = clock_type::now(); }

            std::lock_guard l(frame_mutex_);
            frame_.swap(buf);
            frame_w_ = w;
            frame_h_ = h;
            frame_seq_++;
        }
        else
        {
            dropped_++;
            if (cam->error_code() != ps3eye::camera::ERROR_OK || ++fails >= 3)
            {
                // Unplugged or the stream died. Back off before reopening so
                // we never hammer the USB controller.
                set_status(CameraState::Error, "Lost the camera. Reconnecting...");
                measured_fps_ = 0;
                close();
                std::this_thread::sleep_for(1s);
            }
        }
    }
    close();
}
