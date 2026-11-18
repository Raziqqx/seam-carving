// app.cpp - interactive seam-carving app (bonus 7b), built with Dear ImGui + GLFW + OpenGL.
//
//   seamcarve_app [image]              open the window (optionally with an image loaded)
//   seamcarve_app --selftest <image>   run one resize without opening a window and exit;
//                                      used to check the app's logic on machines with no
//                                      display (for example a CI runner)
//
// The user loads an image, drags the width/height sliders, picks the algorithm and energy,
// and sees the resized image. The result and a picture of all seams can be saved. The view can be
// switched to "all seams on the original", the energy map, or a step-by-step replay with a slider.
//
// Layout of this file:
//   AppState            everything the window shows, in one struct
//   load_image_into     read a file and reset the state
//   run_carve           call the seam-carving library and store the results
//   Texture             an OpenGL texture that shows a cv::Mat
//   draw_controls       the left panel (ImGui widgets)
//   draw_image_panel    the right panel (the picture)
//   main                window set-up and the frame loop
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <exception>
#include <iostream>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <string>
#include <vector>

#include "seam/carve.hpp"
#include "seam/energy.hpp"
#include "seam/visualize.hpp"

namespace {

// What the right-hand panel shows.
enum class ViewMode { Result, AllSeams, Energy, StepByStep, Original };

// Graph cut needs seconds per seam on large images, so the app warns above this size.
const int kGraphCutPixelWarning = 20000;
// The step-by-step replay keeps one picture per seam; skip it for very long runs.
const int kMaxReplayFrames = 600;

struct AppState {
    char path[512] = "";
    std::string status = "Load an image to start.";

    cv::Mat original;                  // the loaded image
    seam::CarveResult result;          // the last resize
    std::vector<cv::Mat> step_frames;  // one picture per removed seam, for the replay
    double last_seconds = 0.0;         // how long the last resize took

    int target_width = 0;
    int target_height = 0;
    int algorithm_index = 0;  // 0 = dp, 1 = greedy, 2 = graphcut
    int energy_index = 0;     // 0 = gradient, 1 = neighbour, 2 = forward
    bool live_update = true;  // re-run the resize whenever a slider is released
    ViewMode view = ViewMode::Result;
    int step_index = 0;  // which replay frame is shown

    bool picture_changed = true;  // the texture must be uploaded again
};

seam::CarveOptions options_of(const AppState& state) {
    const seam::Algorithm algorithms[] = {seam::Algorithm::DynamicProgramming,
                                          seam::Algorithm::Greedy, seam::Algorithm::GraphCut};
    const seam::EnergyMode energies[] = {seam::EnergyMode::Gradient,
                                         seam::EnergyMode::NeighbourDifference,
                                         seam::EnergyMode::Forward};
    return {algorithms[state.algorithm_index], energies[state.energy_index]};
}

// Reads the image at state.path. On success the targets are reset to the image size.
bool load_image_into(AppState& state) {
    const cv::Mat image = cv::imread(state.path, cv::IMREAD_COLOR);
    if (image.empty()) {
        state.status = std::string("Could not read image: ") + state.path;
        return false;
    }
    state.original = image;
    state.result = seam::CarveResult();
    state.result.image = image.clone();
    state.step_frames.clear();
    state.target_width = image.cols;
    state.target_height = image.rows;
    state.step_index = 0;
    state.picture_changed = true;
    state.status = "Loaded " + std::to_string(image.cols) + " x " + std::to_string(image.rows);
    return true;
}

// Runs the resize with the current settings and stores the result in the state.
// Any error (for example forward energy with greedy) is shown in the status line.
void run_carve(AppState& state) {
    if (state.original.empty()) {
        return;
    }
    try {
        const int seam_count = std::abs(state.original.cols - state.target_width) +
                               std::abs(state.original.rows - state.target_height);
        const bool record_steps = seam_count <= kMaxReplayFrames;
        seam::AnimationRecorder recorder(seam::animation_canvas_size(
            state.original.size(), state.target_width, state.target_height));

        const auto start = std::chrono::steady_clock::now();
        state.result =
            seam::carve_to_size(state.original, state.target_width, state.target_height,
                                options_of(state), record_steps ? recorder.callback() : nullptr);
        const auto stop = std::chrono::steady_clock::now();
        state.last_seconds = std::chrono::duration<double>(stop - start).count();

        recorder.add_final_frame(state.result.image);
        state.step_frames = recorder.frames();
        state.step_index = 0;

        char text[160];
        std::snprintf(text, sizeof(text), "%d x %d, %zu seams, total cost %lld, %.3f s",
                      state.result.image.cols, state.result.image.rows, state.result.seams.size(),
                      state.result.total_cost, state.last_seconds);
        state.status = text;
    } catch (const std::exception& error) {
        state.status = std::string("Error: ") + error.what();
    }
    state.picture_changed = true;
}

// The picture for the current view mode.
cv::Mat picture_for_view(const AppState& state) {
    switch (state.view) {
        case ViewMode::Result:
            return state.result.image;
        case ViewMode::AllSeams:
            return seam::draw_all_seams(state.original, state.result.seams);
        case ViewMode::Energy: {
            // Show the energy the user picked. Forward energy is not a per-pixel map (it
            // is computed inside the DP), so for it the gradient map is shown instead.
            seam::EnergyMode mode = options_of(state).energy_mode;
            if (mode == seam::EnergyMode::Forward) {
                mode = seam::EnergyMode::Gradient;
            }
            return seam::colourise_energy(
                seam::compute_energy(seam::to_grayscale(state.original), mode));
        }
        case ViewMode::StepByStep:
            if (!state.step_frames.empty()) {
                return state.step_frames[state.step_index];
            }
            return state.result.image;
        case ViewMode::Original:
            return state.original;
    }
    return state.original;
}

// Saves the original image with every seam of the last resize drawn on it (red = vertical,
// green = horizontal). Returns false if there is nothing to save or the file could not be
// written. Time O(rows * cols + number of seam pixels).
bool save_seams_picture(const AppState& state, const std::string& path) {
    if (state.original.empty()) {
        return false;
    }
    return cv::imwrite(path, seam::draw_all_seams(state.original, state.result.seams));
}

// An OpenGL texture showing one cv::Mat. The destructor frees the texture (RAII).
class Texture {
public:
    Texture() {
        glGenTextures(1, &id_);
    }
    ~Texture() {
        glDeleteTextures(1, &id_);
    }
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    // Copies the picture to the graphics card.
    void upload(const cv::Mat& picture) {
        cv::Mat rgb;
        cv::cvtColor(seam::to_bgr(picture), rgb, cv::COLOR_BGR2RGB);  // OpenGL expects RGB
        glBindTexture(GL_TEXTURE_2D, id_);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);  // our rows are not padded to 4 bytes
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, rgb.cols, rgb.rows, 0, GL_RGB, GL_UNSIGNED_BYTE,
                     rgb.data);
        width_ = rgb.cols;
        height_ = rgb.rows;
    }

    unsigned int id() const {
        return id_;
    }
    int width() const {
        return width_;
    }
    int height() const {
        return height_;
    }

private:
    unsigned int id_ = 0;
    int width_ = 0;
    int height_ = 0;
};

// Left panel: all the controls. Returns after updating `state`.
void draw_controls(AppState& state) {
    ImGui::Begin("Seam carving");

    ImGui::InputText("Image file", state.path, sizeof(state.path));
    if (ImGui::Button("Load")) {
        load_image_into(state);
    }
    ImGui::SameLine();
    if (ImGui::Button("Save result as result.png") && !state.result.image.empty()) {
        const bool saved = cv::imwrite("result.png", state.result.image);
        state.status = saved ? "Saved result.png" : "Could not save result.png";
    }
    ImGui::SameLine();
    if (ImGui::Button("Save seams picture as seams.png") && !state.original.empty()) {
        const bool saved = save_seams_picture(state, "seams.png");
        state.status = saved ? "Saved seams.png" : "Could not save seams.png";
    }
    ImGui::Separator();

    if (!state.original.empty()) {
        bool settings_changed = false;

        ImGui::Text("Original size: %d x %d", state.original.cols, state.original.rows);
        // Shrink down to 1 pixel, or enlarge (seam insertion) up to just under 2x.
        ImGui::SliderInt("Target width", &state.target_width, 1, 2 * state.original.cols - 1);
        settings_changed |= ImGui::IsItemDeactivatedAfterEdit();
        ImGui::SliderInt("Target height", &state.target_height, 1, 2 * state.original.rows - 1);
        settings_changed |= ImGui::IsItemDeactivatedAfterEdit();

        const char* algorithm_names[] = {"Dynamic programming", "Greedy", "Graph cut (slow)"};
        settings_changed |= ImGui::Combo("Algorithm", &state.algorithm_index, algorithm_names, 3);
        const char* energy_names[] = {"Sobel gradient", "Neighbour difference",
                                      "Forward energy (DP only)"};
        settings_changed |= ImGui::Combo("Energy", &state.energy_index, energy_names, 3);

        if (state.algorithm_index == 2 &&
            state.original.rows * state.original.cols > kGraphCutPixelWarning) {
            ImGui::TextWrapped(
                "Warning: graph cut needs about a second per seam or more on an "
                "image of this size. Use a small image, or few seams.");
        }

        if (settings_changed) {
            // The energy view depends on the chosen energy, so redraw it even when no
            // carve runs (live update off, or graph cut selected).
            state.picture_changed = true;
        }

        ImGui::Checkbox("Update when a slider is released", &state.live_update);
        const bool button_pressed = ImGui::Button("Carve now");
        // Graph cut is never started automatically, only by the button.
        const bool auto_run = state.live_update && settings_changed && state.algorithm_index != 2;
        if (button_pressed || auto_run) {
            run_carve(state);
        }
        if (ImGui::Button("Reset size")) {
            state.target_width = state.original.cols;
            state.target_height = state.original.rows;
            run_carve(state);
        }
        ImGui::Separator();

        const char* view_names[] = {"Result", "All seams on the original", "Energy map",
                                    "Step by step", "Original"};
        int view_index = static_cast<int>(state.view);
        if (ImGui::Combo("View", &view_index, view_names, 5)) {
            state.view = static_cast<ViewMode>(view_index);
            state.picture_changed = true;
        }
        if (state.view == ViewMode::StepByStep && !state.step_frames.empty()) {
            const int last = static_cast<int>(state.step_frames.size()) - 1;
            if (ImGui::SliderInt("Seam number", &state.step_index, 0, last)) {
                state.picture_changed = true;
            }
            ImGui::TextWrapped(
                "Red = vertical seam, green = horizontal seam (about to be "
                "removed, or just inserted).");
        }
    }

    ImGui::Separator();
    ImGui::TextWrapped("%s", state.status.c_str());
    ImGui::End();
}

// Right panel: the picture, drawn at its real pixel size.
void draw_image_panel(AppState& state, Texture& texture) {
    ImGui::Begin("Image", nullptr, ImGuiWindowFlags_HorizontalScrollbar);
    if (!state.original.empty()) {
        if (state.picture_changed) {
            texture.upload(picture_for_view(state));
            state.picture_changed = false;
        }
        const ImVec2 size(static_cast<float>(texture.width()),
                          static_cast<float>(texture.height()));
        ImGui::Image(reinterpret_cast<ImTextureID>(static_cast<intptr_t>(texture.id())), size);
    } else {
        ImGui::TextUnformatted("No image loaded.");
    }
    ImGui::End();
}

// Headless check of the app's logic: load, resize with each algorithm, verify the size.
int run_selftest(const std::string& image_path) {
    AppState state;
    std::snprintf(state.path, sizeof(state.path), "%s", image_path.c_str());
    if (!load_image_into(state)) {
        std::cerr << state.status << "\n";
        return 1;
    }
    // One line per check: algorithm (0 = dp, 1 = greedy, 2 = graphcut), change of width,
    // change of height. Negative = shrink, positive = enlarge (seam insertion).
    // Graph cut is slow, so for it the image is first scaled down to 120 pixels wide.
    struct Check {
        int algorithm_index;
        int width_change;
        int height_change;
    };
    const Check checks[] = {
        {0, -20, -10},  // dp shrinks both ways
        {1, -20, -10},  // greedy shrinks both ways
        {0, +15, +8},   // dp enlarges both ways
        {0, -12, +6},   // dp shrinks the width and enlarges the height
        {2, -3, 0},     // graph cut on the small copy
    };
    for (const Check& check : checks) {
        if (check.algorithm_index == 2) {
            const int small_height = state.original.rows * 120 / state.original.cols;
            cv::Mat small;
            cv::resize(state.original, small, cv::Size(120, small_height), 0, 0, cv::INTER_AREA);
            state.original = small;
        }
        state.algorithm_index = check.algorithm_index;
        state.target_width = state.original.cols + check.width_change;
        state.target_height = state.original.rows + check.height_change;
        run_carve(state);
        std::cout << "selftest algorithm " << check.algorithm_index << ": " << state.status << "\n";
        const bool size_ok = state.result.image.cols == state.target_width &&
                             state.result.image.rows == state.target_height;
        const bool reported_error = state.status.rfind("Error", 0) == 0;
        // One replay frame per seam plus the final picture.
        const bool frames_ok = state.step_frames.size() == state.result.seams.size() + 1;
        if (!size_ok || reported_error || !frames_ok) {
            std::cerr << "selftest FAILED\n";
            return 1;
        }
        // Exercise every view once, as the window would.
        for (ViewMode view : {ViewMode::Result, ViewMode::AllSeams, ViewMode::Energy,
                              ViewMode::StepByStep, ViewMode::Original}) {
            state.view = view;
            if (picture_for_view(state).empty()) {
                std::cerr << "selftest FAILED: empty picture\n";
                return 1;
            }
        }
        // The energy view for each energy choice (forward energy shows the gradient map).
        state.view = ViewMode::Energy;
        for (int energy_index = 0; energy_index < 3; ++energy_index) {
            state.energy_index = energy_index;
            if (picture_for_view(state).size() != state.original.size()) {
                std::cerr << "selftest FAILED: energy view " << energy_index << "\n";
                return 1;
            }
        }
        state.energy_index = 0;
        // The "Save seams picture" button: the file must be written and re-readable at the
        // size of the original.
        const std::string seams_path = "selftest_seams.png";
        const cv::Mat reread = save_seams_picture(state, seams_path)
                                   ? cv::imread(seams_path, cv::IMREAD_COLOR)
                                   : cv::Mat();
        std::remove(seams_path.c_str());
        if (reread.size() != state.original.size()) {
            std::cerr << "selftest FAILED: seams picture not saved\n";
            return 1;
        }
    }
    std::cout << "selftest passed\n";
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    const std::vector<std::string> arguments(argv + 1, argv + argc);
    if (arguments.size() == 2 && arguments[0] == "--selftest") {
        return run_selftest(arguments[1]);
    }

    if (!glfwInit()) {
        std::cerr << "could not start GLFW (is a display available?)\n";
        return 1;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    GLFWwindow* window = glfwCreateWindow(1280, 800, "Seam carving - Team 5", nullptr, nullptr);
    if (window == nullptr) {
        std::cerr << "could not create a window\n";
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);  // wait for the screen refresh: no need to draw faster

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    {
        AppState state;
        Texture texture;  // destroyed at the end of this block, before the window closes
        if (!arguments.empty()) {
            std::snprintf(state.path, sizeof(state.path), "%s", arguments[0].c_str());
            load_image_into(state);
        }

        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            draw_controls(state);
            draw_image_panel(state, texture);

            ImGui::Render();
            int display_width = 0;
            int display_height = 0;
            glfwGetFramebufferSize(window, &display_width, &display_height);
            glViewport(0, 0, display_width, display_height);
            glClearColor(0.10f, 0.10f, 0.12f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            glfwSwapBuffers(window);
        }
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
