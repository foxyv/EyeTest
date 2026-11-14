// EyeTest: live preview and settings for the PlayStation Eye camera.
#include "camera_worker.hpp"
#include "presets.hpp"

#include <SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_sdlrenderer2.h>

#include <algorithm>
#include <cstdio>
#include <string>

static void slider(const char* label, int& v, int lo, int hi, int def)
{
    ImGui::PushID(label);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::SameLine(95);
    ImGui::SetNextItemWidth(-ImGui::CalcTextSize("Reset").x - ImGui::GetStyle().ItemSpacing.x -
                            ImGui::GetStyle().FramePadding.x * 2);
    ImGui::SliderInt("##v", &v, lo, hi);
    ImGui::SameLine();
    if (ImGui::SmallButton("Reset")) v = def;
    ImGui::PopID();
}

int main(int, char**)
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "EyeTest", SDL_GetError(), nullptr);
        return 1;
    }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    SDL_Window* window = SDL_CreateWindow("EyeTest - PS3 Eye", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          1120, 620, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!window || !renderer)
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "EyeTest", SDL_GetError(), nullptr);
        return 1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    ImGui::StyleColorsDark();
    ImGui::GetStyle().FrameRounding = 3;
    ImGui::GetStyle().WindowRounding = 0;
    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);

    CameraWorker worker;
    const CameraSettings defaults;
    CameraSettings s;
    CameraMode mode;
    int cam_index = 0;

    SDL_Texture* tex = nullptr;
    int tex_w = 0, tex_h = 0;
    std::vector<uint8_t> frame;
    uint64_t seq = 0;

    std::vector<std::string> preset_names = presets::list();
    int preset_sel = -1;
    char preset_name[64] = "My preset";
    std::string preset_msg;

    bool running = true;
    while (running)
    {
        SDL_Event e;
        while (SDL_PollEvent(&e))
        {
            ImGui_ImplSDL2_ProcessEvent(&e);
            if (e.type == SDL_QUIT) running = false;
        }

        int fw, fh;
        if (worker.latest_frame(frame, fw, fh, seq))
        {
            if (!tex || fw != tex_w || fh != tex_h)
            {
                if (tex) SDL_DestroyTexture(tex);
                tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_BGR24, SDL_TEXTUREACCESS_STREAMING, fw, fh);
                tex_w = fw;
                tex_h = fh;
            }
            SDL_UpdateTexture(tex, nullptr, frame.data(), fw * 3);
        }

        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("main", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                          ImGuiWindowFlags_NoBringToFrontOnFocus);

        const float panel_w = 360;
        const bool streaming = worker.state() == CameraState::Streaming;

        // Preview
        ImGui::BeginChild("preview", ImVec2(-panel_w, 0), ImGuiChildFlags_Border);
        {
            ImVec2 avail = ImGui::GetContentRegionAvail();
            avail.y -= ImGui::GetFrameHeightWithSpacing();
            if (tex && streaming)
            {
                float scale = std::min(avail.x / tex_w, avail.y / tex_h);
                ImVec2 sz(tex_w * scale, tex_h * scale);
                ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + (avail.x - sz.x) / 2,
                                           ImGui::GetCursorPosY() + (avail.y - sz.y) / 2));
                ImGui::Image((ImTextureID)(intptr_t)tex, sz);
            }
            else
            {
                ImGui::Dummy(ImVec2(0, avail.y / 2 - 20));
                ImGui::PushTextWrapPos(0);
                ImGui::TextColored(worker.state() == CameraState::Searching ? ImVec4(.8f, .8f, .8f, 1)
                                                                            : ImVec4(1, .55f, .4f, 1),
                                   "%s", worker.status().c_str());
                ImGui::PopTextWrapPos();
                ImGui::Dummy(ImVec2(0, avail.y / 2 - 20));
            }
            ImGui::SetCursorPosY(ImGui::GetWindowHeight() - ImGui::GetFrameHeightWithSpacing());
            if (streaming)
                ImGui::Text("%dx%d   set %d Hz   measured %.1f fps   dropped %llu", tex_w, tex_h,
                            worker.actual_fps(), worker.measured_fps(),
                            (unsigned long long)worker.dropped_frames());
            else
                ImGui::TextDisabled("Not streaming");
        }
        ImGui::EndChild();

        ImGui::SameLine();

        // Controls
        ImGui::BeginChild("controls", ImVec2(0, 0), ImGuiChildFlags_Border);
        {
            ImGui::SeparatorText("Camera");
            int count = std::max(1, worker.camera_count());
            ImGui::SetNextItemWidth(-1);
            char cam_label[32];
            snprintf(cam_label, sizeof cam_label, "Camera %d", cam_index + 1);
            if (ImGui::BeginCombo("##cam", cam_label))
            {
                for (int i = 0; i < count; i++)
                {
                    char l[32];
                    snprintf(l, sizeof l, "Camera %d", i + 1);
                    if (ImGui::Selectable(l, i == cam_index)) cam_index = i;
                }
                ImGui::EndCombo();
            }

            int res = mode.vga ? 0 : 1;
            ImGui::Text("Resolution");
            ImGui::SameLine(110);
            if (ImGui::RadioButton("640x480", res == 0)) res = 0;
            ImGui::SameLine();
            if (ImGui::RadioButton("320x240", res == 1)) res = 1;
            if ((res == 0) != mode.vga)
            {
                mode.vga = res == 0;
                auto& r = CameraWorker::framerates(mode.vga);
                if (std::find(r.begin(), r.end(), mode.fps) == r.end()) mode.fps = r.back() >= 60 ? 60 : r.back();
            }
            ImGui::Text("Frame rate");
            ImGui::SameLine(110);
            ImGui::SetNextItemWidth(-1);
            char fps_label[16];
            snprintf(fps_label, sizeof fps_label, "%d Hz", mode.fps);
            if (ImGui::BeginCombo("##fps", fps_label))
            {
                for (int f : CameraWorker::framerates(mode.vga))
                {
                    char l[16];
                    snprintf(l, sizeof l, "%d Hz", f);
                    if (ImGui::Selectable(l, f == mode.fps)) mode.fps = f;
                }
                ImGui::EndCombo();
            }

            ImGui::SeparatorText("Exposure");
            slider("Exposure", s.exposure, 0, 255, defaults.exposure);
            ImGui::BeginDisabled(s.auto_gain);
            slider("Gain", s.gain, 0, 63, defaults.gain);
            ImGui::EndDisabled();
            ImGui::Checkbox("Auto gain", &s.auto_gain);

            ImGui::SeparatorText("Image");
            slider("Brightness", s.brightness, 0, 255, defaults.brightness);
            slider("Contrast", s.contrast, 0, 255, defaults.contrast);
            slider("Saturation", s.saturation, 0, 255, defaults.saturation);
            slider("Hue", s.hue, 0, 128, defaults.hue);
            slider("Sharpness", s.sharpness, 0, 63, defaults.sharpness);

            ImGui::SeparatorText("White balance");
            ImGui::Checkbox("Auto white balance", &s.awb);
            ImGui::BeginDisabled(s.awb);
            slider("Red", s.red_balance, 0, 255, defaults.red_balance);
            slider("Green", s.green_balance, 0, 255, defaults.green_balance);
            slider("Blue", s.blue_balance, 0, 255, defaults.blue_balance);
            ImGui::EndDisabled();

            ImGui::SeparatorText("Other");
            ImGui::Checkbox("Flip horizontal", &s.flip_h);
            ImGui::SameLine();
            ImGui::Checkbox("Flip vertical", &s.flip_v);
            ImGui::Checkbox("Test pattern", &s.test_pattern);

            ImGui::Spacing();
            if (ImGui::Button("Reset all to defaults", ImVec2(-1, 0)))
                s = defaults;

            ImGui::SeparatorText("Presets");
            ImGui::SetNextItemWidth(-70);
            ImGui::InputText("##pname", preset_name, sizeof preset_name);
            ImGui::SameLine();
            if (ImGui::Button("Save", ImVec2(-1, 0)))
            {
                preset_msg = presets::save(preset_name, s, mode) ? "Saved." : "Couldn't save the preset.";
                preset_names = presets::list();
            }
            ImGui::SetNextItemWidth(-70);
            const char* sel = preset_sel >= 0 && preset_sel < (int)preset_names.size()
                                  ? preset_names[preset_sel].c_str() : "Choose a preset";
            if (ImGui::BeginCombo("##plist", sel))
            {
                for (int i = 0; i < (int)preset_names.size(); i++)
                    if (ImGui::Selectable(preset_names[i].c_str(), i == preset_sel)) preset_sel = i;
                ImGui::EndCombo();
            }
            ImGui::SameLine();
            ImGui::BeginDisabled(preset_sel < 0);
            if (ImGui::Button("Load", ImVec2(-1, 0)) && preset_sel >= 0)
            {
                preset_msg = presets::load(preset_names[preset_sel], s, mode) ? "Loaded." : "Couldn't load the preset.";
                snprintf(preset_name, sizeof preset_name, "%s", preset_names[preset_sel].c_str());
            }
            ImGui::EndDisabled();
            if (!preset_msg.empty()) ImGui::TextDisabled("%s", preset_msg.c_str());
        }
        ImGui::EndChild();
        ImGui::End();

        worker.set_settings(s);
        worker.set_mode(mode);
        worker.set_camera_index(cam_index);

        ImGui::Render();
        SDL_SetRenderDrawColor(renderer, 20, 22, 26, 255);
        SDL_RenderClear(renderer);
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);
    }

    if (tex) SDL_DestroyTexture(tex);
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
