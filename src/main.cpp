// main.cpp - command-line tool.
//
//   seamcarve [options] <input image> <output image>
//
// Example: make an image 100 pixels narrower with DP and save the seam pictures:
//   seamcarve --algo dp --width 400 --seams-image seams.png --video steps.mp4 in.jpg out.png
#include <chrono>
#include <exception>
#include <iostream>
#include <opencv2/imgcodecs.hpp>
#include <stdexcept>
#include <string>
#include <vector>

#include "seam/carve.hpp"
#include "seam/cli_options.hpp"
#include "seam/energy.hpp"
#include "seam/visualize.hpp"

namespace {

// Everything the user can set on the command line.
struct Arguments {
    std::string input_path;
    std::string output_path;
    seam::CarveOptions options;
    int target_width = -1;          // -1 = keep the current width
    int target_height = -1;         // -1 = keep the current height
    std::string seams_image_path;   // overlay of all seams on the original (optional)
    std::string video_path;         // step-by-step MP4 (optional)
    std::string frames_prefix;      // step-by-step PNG frames (optional)
    std::string energy_image_path;  // colour picture of the energy map (optional)
    int frame_step = 1;             // keep every n-th PNG frame
    bool show_help = false;
};

void print_usage() {
    std::cout << "Usage: seamcarve [options] <input image> <output image>\n"
                 "\n"
                 "Options:\n"
                 "  --algo dp|greedy|graphcut   seam finder (default dp)\n"
                 "  --energy gradient|neighbour|forward\n"
                 "                              energy function (default gradient;\n"
                 "                              forward works with --algo dp only)\n"
                 "  --width W                   target width in pixels (default: unchanged)\n"
                 "  --height H                  target height in pixels (default: unchanged)\n"
                 "                              a target larger than the image inserts seams\n"
                 "  --seams-image FILE          save the original with every seam drawn on it\n"
                 "  --video FILE.mp4            save a step-by-step animation\n"
                 "  --frames PREFIX             save the animation as PREFIX_0000.png, ...\n"
                 "  --frame-step N              with --frames, keep every N-th frame (default 1)\n"
                 "  --energy-image FILE         save a colour picture of the energy map\n"
                 "  --help                      show this text\n";
}

// Reads the value that follows an option such as "--width 300".
std::string take_value(const std::vector<std::string>& tokens, size_t& index) {
    if (index + 1 >= tokens.size()) {
        throw std::invalid_argument("missing value after " + tokens[index]);
    }
    ++index;
    return tokens[index];
}

// Converts the value of a size option such as "--width 300" to a whole number >= 1.
// Rejects "abc", "10abc" (trailing characters) and values below 1 with a clear message,
// so a bad size never reaches the carving code. Time O(length of text).
int parse_positive_int(const std::string& text, const std::string& option) {
    size_t characters_read = 0;
    int value = 0;
    try {
        value = std::stoi(text, &characters_read);
    } catch (const std::exception&) {
        throw std::invalid_argument(option + " needs a whole number, got \"" + text + "\"");
    }
    if (characters_read != text.size()) {
        throw std::invalid_argument(option + " needs a whole number, got \"" + text + "\"");
    }
    if (value < 1) {
        throw std::invalid_argument(option + " must be at least 1, got " + text);
    }
    return value;
}

Arguments parse_arguments(const std::vector<std::string>& tokens) {
    Arguments arguments;
    std::vector<std::string> positional;

    for (size_t index = 0; index < tokens.size(); ++index) {
        const std::string& token = tokens[index];
        if (token == "--help" || token == "-h") {
            arguments.show_help = true;
        } else if (token == "--algo") {
            arguments.options.algorithm = seam::parse_algorithm(take_value(tokens, index));
        } else if (token == "--energy") {
            arguments.options.energy_mode = seam::parse_energy_mode(take_value(tokens, index));
        } else if (token == "--width") {
            arguments.target_width = parse_positive_int(take_value(tokens, index), token);
        } else if (token == "--height") {
            arguments.target_height = parse_positive_int(take_value(tokens, index), token);
        } else if (token == "--seams-image") {
            arguments.seams_image_path = take_value(tokens, index);
        } else if (token == "--video") {
            arguments.video_path = take_value(tokens, index);
        } else if (token == "--frames") {
            arguments.frames_prefix = take_value(tokens, index);
        } else if (token == "--frame-step") {
            arguments.frame_step = parse_positive_int(take_value(tokens, index), token);
        } else if (token == "--energy-image") {
            arguments.energy_image_path = take_value(tokens, index);
        } else if (token.rfind("--", 0) == 0) {
            throw std::invalid_argument("unknown option " + token);
        } else {
            positional.push_back(token);
        }
    }

    if (arguments.show_help) {
        return arguments;
    }
    if (positional.size() != 2) {
        throw std::invalid_argument("expected exactly two file names: <input> <output>");
    }
    if (arguments.frame_step < 1) {
        throw std::invalid_argument("--frame-step must be at least 1");
    }
    arguments.input_path = positional[0];
    arguments.output_path = positional[1];
    return arguments;
}

// Saves an image and fails loudly if OpenCV could not write it.
void save_image(const std::string& path, const cv::Mat& image) {
    if (!cv::imwrite(path, image)) {
        throw std::runtime_error("could not write " + path);
    }
}

// Writes the recorded animation as a video and/or as PNG frames, as the user asked.
void save_animation(const Arguments& arguments, const std::vector<cv::Mat>& frames) {
    if (!arguments.video_path.empty() && !seam::write_video(arguments.video_path, frames, 30.0)) {
        throw std::runtime_error("could not write video " + arguments.video_path);
    }
    if (!arguments.frames_prefix.empty()) {
        seam::write_frames(arguments.frames_prefix, frames, arguments.frame_step);
    }
}

int run(const Arguments& arguments) {
    const cv::Mat image = cv::imread(arguments.input_path, cv::IMREAD_COLOR);
    if (image.empty()) {
        throw std::runtime_error("could not read image " + arguments.input_path);
    }

    const int target_width = (arguments.target_width > 0) ? arguments.target_width : image.cols;
    const int target_height = (arguments.target_height > 0) ? arguments.target_height : image.rows;

    if (!arguments.energy_image_path.empty()) {
        const seam::EnergyMode map_mode =
            (arguments.options.energy_mode == seam::EnergyMode::Forward)
                ? seam::EnergyMode::Gradient
                : arguments.options.energy_mode;
        const seam::EnergyMap energy = seam::compute_energy(seam::to_grayscale(image), map_mode);
        save_image(arguments.energy_image_path, seam::colourise_energy(energy));
    }

    const auto start = std::chrono::steady_clock::now();
    seam::CarveResult result;
    const bool want_animation = !arguments.video_path.empty() || !arguments.frames_prefix.empty();
    if (want_animation) {
        // Record one frame per seam while carving, then save the frames.
        seam::AnimationRecorder recorder(
            seam::animation_canvas_size(image.size(), target_width, target_height));
        result = seam::carve_to_size(image, target_width, target_height, arguments.options,
                                     recorder.callback());
        recorder.add_final_frame(result.image);
        save_animation(arguments, recorder.frames());
    } else {
        // No animation asked for: do not record frames, they use a lot of memory.
        result = seam::carve_to_size(image, target_width, target_height, arguments.options);
    }
    const auto stop = std::chrono::steady_clock::now();
    const double seconds = std::chrono::duration<double>(stop - start).count();

    save_image(arguments.output_path, result.image);
    if (!arguments.seams_image_path.empty()) {
        save_image(arguments.seams_image_path, seam::draw_all_seams(image, result.seams));
    }

    std::cout << "algorithm:    " << seam::to_string(arguments.options.algorithm) << "\n"
              << "energy:       " << seam::to_string(arguments.options.energy_mode) << "\n"
              << "input size:   " << image.cols << " x " << image.rows << "\n"
              << "output size:  " << result.image.cols << " x " << result.image.rows << "\n"
              << "seams:        " << result.seams.size() << "\n"
              << "total cost:   " << result.total_cost << "\n"
              << "time:         " << seconds << " s\n";
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const std::vector<std::string> tokens(argv + 1, argv + argc);
        const Arguments arguments = parse_arguments(tokens);
        if (arguments.show_help) {
            print_usage();
            return 0;
        }
        return run(arguments);
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << "\n";
        std::cerr << "run 'seamcarve --help' for usage\n";
        return 1;
    }
}
