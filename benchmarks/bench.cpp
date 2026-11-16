// bench.cpp - produces every number and every result picture used in the report.
//
//   seam_bench <experiment> <inputs dir> <output dir>
//
// Experiments (each writes CSV files into <output dir>/csv, "visual" writes pictures).
// The timing experiments size, seams and improve are repeated 3 times (column
// "repetition"); the report uses the median and quotes the range.
//   size     runtime of ONE seam search for growing image sizes
//   seams    cumulative runtime while removing more and more seams
//   cost     total seam cost and runtime of each algorithm on the three test images
//   improve  item 5d experiments: memory, OpenMP threads, incremental energy
//   memory   bytes per pixel of the graph-cut network and of the DP tables
//   visual   result pictures: seam carving vs scaling vs cropping, seam overlays, video
//   all      everything above
#include <algorithm>
#include <chrono>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <stdexcept>
#include <string>
#include <vector>

#include "seam/carve.hpp"
#include "seam/dp.hpp"
#include "seam/energy.hpp"
#include "seam/graphcut.hpp"
#include "seam/greedy.hpp"
#include "seam/improvements.hpp"
#include "seam/visualize.hpp"

namespace {

using seam::Algorithm;
using seam::CarveOptions;
using seam::CarveResult;
using seam::Direction;
using seam::EnergyMap;
using seam::EnergyMode;
using seam::Seam;

const std::vector<std::string> kImageNames = {"tower", "bridge", "castle"};
const Algorithm kAllAlgorithms[] = {Algorithm::DynamicProgramming, Algorithm::Greedy,
                                    Algorithm::GraphCut};

// Width of the small copies used whenever graph cut takes part in a comparison
// (graph cut is far too slow for the full 500-pixel-wide images).
const int kSmallWidth = 150;

// Every timing experiment is run this many times from scratch. The report uses the median
// of the repetitions and quotes their range, because single timings vary from run to run.
const int kRepetitions = 3;

// ---------------------------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------------------------

// Runs `work` once and returns how long it took, in milliseconds.
double time_once_ms(const std::function<void()>& work) {
    const auto start = std::chrono::steady_clock::now();
    work();
    const auto stop = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(stop - start).count();
}

// Runs `work` `repeats` times and returns the MEDIAN time in milliseconds. The median
// ignores the occasional slow run caused by other programs on the machine.
double median_time_ms(int repeats, const std::function<void()>& work) {
    std::vector<double> times;
    for (int run = 0; run < repeats; ++run) {
        times.push_back(time_once_ms(work));
    }
    std::sort(times.begin(), times.end());
    return times[times.size() / 2];
}

cv::Mat load_image(const std::string& inputs_dir, const std::string& name) {
    const std::string path = inputs_dir + "/" + name + ".png";
    const cv::Mat image = cv::imread(path, cv::IMREAD_COLOR);
    if (image.empty()) {
        throw std::runtime_error("could not read " + path);
    }
    return image;
}

// Plain (content-unaware) scaling to a given width, keeping the aspect ratio.
cv::Mat scale_to_width(const cv::Mat& image, int width) {
    const int height = std::max(1, image.rows * width / image.cols);
    cv::Mat scaled;
    cv::resize(image, scaled, cv::Size(width, height), 0, 0, cv::INTER_AREA);
    return scaled;
}

// Opens <output dir>/csv/<name>. The timing experiments are repeated kRepetitions times:
// repetition 1 starts a new file and writes the header line, later ones append to it.
std::ofstream open_csv(const std::string& output_dir, const std::string& name,
                       const std::string& header, int repetition = 1) {
    std::filesystem::create_directories(output_dir + "/csv");
    const std::string path = output_dir + "/csv/" + name;
    std::ofstream file(path, repetition == 1 ? std::ios::trunc : std::ios::app);
    if (!file) {
        throw std::runtime_error("could not write " + path);
    }
    if (repetition == 1) {
        file << header << "\n";
    }
    return file;
}

void save(const std::string& path, const cv::Mat& image) {
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());
    if (!cv::imwrite(path, image)) {
        throw std::runtime_error("could not write " + path);
    }
}

Seam find_seam_in_energy(Algorithm algorithm, const EnergyMap& energy) {
    switch (algorithm) {
        case Algorithm::DynamicProgramming:
            return seam::find_vertical_seam_dp(energy);
        case Algorithm::Greedy:
            return seam::find_vertical_seam_greedy(energy);
        case Algorithm::GraphCut:
            return seam::find_vertical_seam_graphcut(energy);
    }
    return {};
}

// Sum of the Sobel energy of every pixel of an image.
long long total_energy(const cv::Mat& image) {
    return static_cast<long long>(
        cv::sum(seam::compute_gradient_energy(seam::to_grayscale(image)))[0]);
}

// ---------------------------------------------------------------------------------------
// Experiment "size": one seam search on images of growing size
// ---------------------------------------------------------------------------------------
void run_size_experiment(const std::string& inputs_dir, const std::string& output_dir,
                         int repetition) {
    std::ofstream csv = open_csv(
        output_dir, "runtime_vs_size.csv",
        "repetition,algorithm,rows,cols,pixels,energy_ms,seam_ms,augmentations", repetition);
    const cv::Mat base = load_image(inputs_dir, "tower");

    const std::vector<int> fast_widths = {100, 200, 400, 500, 800, 1600, 3200};
    // The last graph-cut size is a full test image (one timing per repetition), to
    // document how impractical it is.
    const std::vector<int> graphcut_widths = {25, 50, 75, 100, 150, 200, 250, 500};

    for (Algorithm algorithm : kAllAlgorithms) {
        const bool is_graphcut = (algorithm == Algorithm::GraphCut);
        const std::vector<int>& widths = is_graphcut ? graphcut_widths : fast_widths;
        for (int width : widths) {
            const int repeats = !is_graphcut ? 25 : (width < 500 ? 3 : 1);
            const cv::Mat_<uchar> gray = seam::to_grayscale(scale_to_width(base, width));
            EnergyMap energy;
            const double energy_ms =
                median_time_ms(repeats, [&] { energy = seam::compute_gradient_energy(gray); });
            const double seam_ms =
                median_time_ms(repeats, [&] { find_seam_in_energy(algorithm, energy); });

            long long augmentations = 0;
            if (is_graphcut) {
                augmentations = seam::find_vertical_seam_graphcut_detailed(energy).augmentations;
            }
            csv << repetition << "," << seam::to_string(algorithm) << "," << gray.rows << ","
                << gray.cols << "," << gray.rows * gray.cols << "," << energy_ms << "," << seam_ms
                << "," << augmentations << "\n";
            std::cout << "size: " << seam::to_string(algorithm) << " " << gray.cols << "x"
                      << gray.rows << " seam " << seam_ms << " ms" << std::endl;
        }
    }
}

// ---------------------------------------------------------------------------------------
// Experiment "seams": cumulative time while removing k seams
// ---------------------------------------------------------------------------------------
void run_seams_experiment(const std::string& inputs_dir, const std::string& output_dir,
                          int repetition) {
    std::ofstream csv =
        open_csv(output_dir, "runtime_vs_seams.csv",
                 "repetition,algorithm,image,rows,cols,seams_removed,cumulative_ms", repetition);
    const cv::Mat full = load_image(inputs_dir, "tower");
    const cv::Mat small = scale_to_width(full, kSmallWidth);

    for (Algorithm algorithm : kAllAlgorithms) {
        // Graph cut runs on the small copy only; DP and greedy run on both sizes so that
        // the three methods can be compared on the same (small) picture.
        std::vector<std::pair<std::string, cv::Mat>> inputs = {{"tower_small", small}};
        if (algorithm != Algorithm::GraphCut) {
            inputs.push_back({"tower", full});
        }

        for (const auto& [name, image] : inputs) {
            CarveOptions options;
            options.algorithm = algorithm;
            const int total = image.cols / 2;     // remove half of the columns
            const int report_every = total / 10;  // ten data points per curve

            cv::Mat current = image.clone();
            double elapsed_ms = 0.0;
            for (int removed = 1; removed <= total; ++removed) {
                elapsed_ms += time_once_ms([&] {
                    const Seam seam = seam::find_vertical_seam(current, options);
                    current = seam::remove_vertical_seam(current, seam);
                });
                if (removed % report_every == 0) {
                    csv << repetition << "," << seam::to_string(algorithm) << "," << name << ","
                        << image.rows << "," << image.cols << "," << removed << "," << elapsed_ms
                        << "\n";
                }
            }
            std::cout << "seams: " << seam::to_string(algorithm) << " " << name << " " << total
                      << " seams in " << elapsed_ms << " ms" << std::endl;
        }
    }
}

// ---------------------------------------------------------------------------------------
// Experiment "cost": total seam cost per algorithm, image and direction
// ---------------------------------------------------------------------------------------

// Removes `count` seams and returns the result; also counts on how many steps the chosen
// seam had exactly the optimal (DP) cost for the image of that step.
CarveResult carve_and_count_optimal(const cv::Mat& image, int count, Direction direction,
                                    const CarveOptions& options, int& optimal_steps) {
    optimal_steps = 0;
    const seam::StepCallback check = [&](const cv::Mat& before, const seam::SeamRecord& record) {
        long long dp_cost = 0;
        CarveOptions dp_options = options;
        dp_options.algorithm = Algorithm::DynamicProgramming;
        seam::find_seam(before, direction, dp_options, &dp_cost);
        if (record.cost == dp_cost) {
            ++optimal_steps;
        }
    };
    return seam::remove_seams(image, count, direction, options, check);
}

void run_cost_experiment(const std::string& inputs_dir, const std::string& output_dir) {
    std::ofstream csv =
        open_csv(output_dir, "seam_cost.csv",
                 "image,rows,cols,direction,algorithm,energy,seams,first_seam_cost,total_cost,"
                 "optimal_steps,time_ms,image_energy_before,image_energy_after");

    for (const std::string& name : kImageNames) {
        const cv::Mat full = load_image(inputs_dir, name);
        const std::vector<std::pair<std::string, cv::Mat>> inputs = {
            {name, full}, {name + "_small", scale_to_width(full, kSmallWidth)}};

        for (const auto& [label, image] : inputs) {
            const bool is_small = (image.cols == kSmallWidth);
            for (Direction direction : {Direction::Vertical, Direction::Horizontal}) {
                const int size = (direction == Direction::Vertical) ? image.cols : image.rows;
                const int count = size * 3 / 10;  // remove 30 %

                for (Algorithm algorithm : kAllAlgorithms) {
                    if (algorithm == Algorithm::GraphCut && !is_small) {
                        continue;  // too slow on the full-size images
                    }
                    for (EnergyMode mode : {EnergyMode::Gradient, EnergyMode::Forward}) {
                        if (mode == EnergyMode::Forward &&
                            algorithm != Algorithm::DynamicProgramming) {
                            continue;  // forward energy is DP-only
                        }
                        CarveOptions options;
                        options.algorithm = algorithm;
                        options.energy_mode = mode;

                        CarveResult result;
                        int optimal_steps = 0;
                        // Timed run without the checking callback...
                        const double time_ms = time_once_ms(
                            [&] { result = seam::remove_seams(image, count, direction, options); });
                        // ...then an untimed run that counts the optimal steps.
                        carve_and_count_optimal(image, count, direction, options, optimal_steps);

                        csv << label << "," << image.rows << "," << image.cols << ","
                            << seam::to_string(direction) << "," << seam::to_string(algorithm)
                            << "," << seam::to_string(mode) << "," << count << ","
                            << result.seams.front().cost << "," << result.total_cost << ","
                            << optimal_steps << "," << time_ms << "," << total_energy(image) << ","
                            << total_energy(result.image) << "\n";
                        std::cout << "cost: " << label << " " << seam::to_string(direction) << " "
                                  << seam::to_string(algorithm) << " " << seam::to_string(mode)
                                  << " total " << result.total_cost << std::endl;
                    }
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------------------
// Experiment "improve": spec item 5d
// ---------------------------------------------------------------------------------------
void run_improve_experiment(const std::string& inputs_dir, const std::string& output_dir,
                            int repetition) {
    const cv::Mat base = load_image(inputs_dir, "tower");
    const cv::Mat_<uchar> big_gray = seam::to_grayscale(scale_to_width(base, 3000));
    const EnergyMap big_energy = seam::compute_gradient_energy(big_gray);
    const int repeats = 15;

    // (1) Do we need the whole DP table? Full tables vs two rolling rows + 1-byte steps.
    {
        std::ofstream csv =
            open_csv(output_dir, "improve_memory.csv",
                     "repetition,variant,rows,cols,table_bytes,seam_ms", repetition);
        const long long pixels = static_cast<long long>(big_energy.rows) * big_energy.cols;
        const long long full_bytes = pixels * 2 * static_cast<long long>(sizeof(int));
        const long long rolling_bytes = pixels * static_cast<long long>(sizeof(signed char)) +
                                        2LL * big_energy.cols * static_cast<long long>(sizeof(int));
        const double full_ms =
            median_time_ms(repeats, [&] { seam::find_vertical_seam_dp(big_energy); });
        const double rolling_ms =
            median_time_ms(repeats, [&] { seam::find_vertical_seam_dp_rolling(big_energy); });
        csv << repetition << ",full_table," << big_energy.rows << "," << big_energy.cols << ","
            << full_bytes << "," << full_ms << "\n";
        csv << repetition << ",rolling_rows," << big_energy.rows << "," << big_energy.cols << ","
            << rolling_bytes << "," << rolling_ms << "\n";
        std::cout << "improve: memory done" << std::endl;
    }

    // (2) Does multithreading help? Energy map and DP table with 1..16 threads.
    {
        std::ofstream csv = open_csv(output_dir, "improve_threads.csv",
                                     "repetition,stage,rows,cols,threads,time_ms", repetition);
        // Two sizes: threads pay off only when a row holds enough work.
        for (int width : {500, 3000}) {
            const cv::Mat_<uchar> gray = seam::to_grayscale(scale_to_width(base, width));
            const EnergyMap energy = seam::compute_gradient_energy(gray);

            // For reference: OpenCV's own Sobel (the default energy path of the library).
            csv << repetition << ",energy_opencv," << gray.rows << "," << gray.cols << ",0,"
                << median_time_ms(repeats, [&] { seam::compute_gradient_energy(gray); }) << "\n";

            // Thread count 0 marks the plain serial function (no OpenMP involved). The
            // speed-ups in the report are relative to these two hand-written loops.
            csv << repetition << ",energy," << gray.rows << "," << gray.cols << ",0,"
                << median_time_ms(repeats, [&] { seam::compute_gradient_energy_manual(gray); })
                << "\n";
            csv << repetition << ",dp_table," << gray.rows << "," << gray.cols << ",0,"
                << median_time_ms(repeats, [&] { seam::build_cumulative_energy(energy); }) << "\n";

            for (int threads : {1, 2, 4, 8, 16}) {
                csv << repetition << ",energy," << gray.rows << "," << gray.cols << "," << threads
                    << ","
                    << median_time_ms(
                           repeats, [&] { seam::compute_gradient_energy_parallel(gray, threads); })
                    << "\n";
                csv << repetition << ",dp_table," << gray.rows << "," << gray.cols << "," << threads
                    << ","
                    << median_time_ms(
                           repeats,
                           [&] { seam::build_cumulative_energy_parallel(energy, threads); })
                    << "\n";
            }
        }
        std::cout << "improve: threads done" << std::endl;
    }

    // (3) Incremental energy update vs recomputing the whole map after every seam.
    {
        std::ofstream csv =
            open_csv(output_dir, "improve_incremental.csv",
                     "repetition,variant,rows,cols,seams,energy_ms,total_ms", repetition);
        const cv::Mat_<uchar> start_gray = seam::to_grayscale(scale_to_width(base, 1000));
        const int seams = 200;

        for (const bool incremental : {false, true}) {
            cv::Mat_<uchar> gray = start_gray.clone();
            EnergyMap energy = seam::compute_gradient_energy_manual(gray);
            double energy_ms = 0.0;
            const double total_ms = time_once_ms([&] {
                for (int removed = 0; removed < seams; ++removed) {
                    const Seam seam = seam::find_vertical_seam_dp(energy);
                    gray = cv::Mat_<uchar>(seam::remove_vertical_seam(gray, seam));
                    energy_ms += time_once_ms([&] {
                        if (incremental) {
                            energy = seam::update_gradient_energy_after_removal(energy, gray, seam);
                        } else {
                            // Same hand-written Sobel as the incremental path uses, so the
                            // comparison is fair.
                            energy = seam::compute_gradient_energy_manual(gray);
                        }
                    });
                }
            });
            csv << repetition << "," << (incremental ? "incremental" : "full_recompute") << ","
                << start_gray.rows << "," << start_gray.cols << "," << seams << "," << energy_ms
                << "," << total_ms << "\n";
        }
        std::cout << "improve: incremental done" << std::endl;
    }
}

// ---------------------------------------------------------------------------------------
// Experiment "memory": bytes per pixel of the graph-cut network and of the DP tables
// ---------------------------------------------------------------------------------------
void run_memory_experiment(const std::string& inputs_dir, const std::string& output_dir) {
    std::ofstream csv = open_csv(output_dir, "memory_per_pixel.csv",
                                 "structure,rows,cols,nodes,edges,bytes,bytes_per_pixel");
    const cv::Mat small = scale_to_width(load_image(inputs_dir, "tower"), kSmallWidth);
    const EnergyMap energy = seam::compute_gradient_energy(seam::to_grayscale(small));
    const double pixels = static_cast<double>(energy.rows) * energy.cols;

    const seam::FlowNetwork network = seam::build_seam_graph(energy, seam::SinkLink::EnergyToSink);
    csv << "graph_cut_network," << energy.rows << "," << energy.cols << "," << network.node_count()
        << "," << network.edge_count() << "," << network.memory_bytes() << ","
        << static_cast<double>(network.memory_bytes()) / pixels << "\n";

    // Full DP tables: one int for the cumulative cost and one for the parent, per pixel.
    const long long dp_bytes = static_cast<long long>(pixels) * 2 * sizeof(int);
    csv << "dp_full_tables," << energy.rows << "," << energy.cols << ",0,0," << dp_bytes << ","
        << static_cast<double>(dp_bytes) / pixels << "\n";
    std::cout << "memory: done" << std::endl;
}

// ---------------------------------------------------------------------------------------
// Experiment "visual": result pictures for the report and for XSite "Other Materials"
// ---------------------------------------------------------------------------------------

// Cropping baseline: keep the centre of the image.
cv::Mat crop_centre(const cv::Mat& image, int width, int height) {
    const int left = (image.cols - width) / 2;
    const int top = (image.rows - height) / 2;
    return image(cv::Rect(left, top, width, height)).clone();
}

// Scaling baseline: squeeze the whole image into the target size.
cv::Mat scale_to_size(const cv::Mat& image, int width, int height) {
    cv::Mat scaled;
    cv::resize(image, scaled, cv::Size(width, height), 0, 0, cv::INTER_AREA);
    return scaled;
}

void run_visual_experiment(const std::string& inputs_dir, const std::string& output_dir) {
    for (const std::string& name : kImageNames) {
        const cv::Mat image = load_image(inputs_dir, name);
        const std::string dir = output_dir + "/" + name + "/";
        const int target_width = image.cols * 7 / 10;  // 30 % narrower

        save(dir + "original.png", image);
        save(dir + "energy.png",
             seam::colourise_energy(seam::compute_gradient_energy(seam::to_grayscale(image))));
        save(dir + "scaled.png", scale_to_size(image, target_width, image.rows));
        save(dir + "cropped.png", crop_centre(image, target_width, image.rows));

        // DP (gradient), DP (forward energy) and greedy on the full image.
        const std::vector<std::pair<std::string, CarveOptions>> runs = {
            {"dp", {Algorithm::DynamicProgramming, EnergyMode::Gradient}},
            {"dp_forward", {Algorithm::DynamicProgramming, EnergyMode::Forward}},
            {"greedy", {Algorithm::Greedy, EnergyMode::Gradient}},
        };
        for (const auto& [label, options] : runs) {
            const CarveResult result =
                seam::carve_to_size(image, target_width, image.rows, options);
            save(dir + label + ".png", result.image);
            save(dir + label + "_seams.png", seam::draw_all_seams(image, result.seams));
        }

        // Horizontal seams and seam insertion (DP).
        const CarveOptions dp_options;
        const CarveResult shorter =
            seam::carve_to_size(image, image.cols, image.rows * 7 / 10, dp_options);
        save(dir + "dp_horizontal.png", shorter.image);
        save(dir + "dp_horizontal_seams.png", seam::draw_all_seams(image, shorter.seams));
        const CarveResult wider =
            seam::carve_to_size(image, image.cols * 12 / 10, image.rows, dp_options);
        save(dir + "dp_enlarged.png", wider.image);
        save(dir + "dp_enlarged_seams.png", seam::draw_all_seams(image, wider.seams));
        // Narrower AND taller in one call: removal in one direction, insertion in the other.
        const CarveResult mixed =
            seam::carve_to_size(image, image.cols * 8 / 10, image.rows * 115 / 100, dp_options);
        save(dir + "dp_mixed.png", mixed.image);
        save(dir + "dp_mixed_seams.png", seam::draw_all_seams(image, mixed.seams));

        // All three algorithms on the small copy (graph cut is practical only here).
        const cv::Mat small = scale_to_width(image, kSmallWidth);
        save(dir + "small_original.png", small);
        for (Algorithm algorithm : kAllAlgorithms) {
            CarveOptions options;
            options.algorithm = algorithm;
            const CarveResult result =
                seam::carve_to_size(small, kSmallWidth * 7 / 10, small.rows, options);
            const std::string label = "small_" + seam::to_string(algorithm);
            save(dir + label + ".png", result.image);
            save(dir + label + "_seams.png", seam::draw_all_seams(small, result.seams));
        }
        std::cout << "visual: " << name << " done" << std::endl;
    }

    // Step-by-step animation (bonus 7a): width and height of the tower image with DP.
    const cv::Mat tower = load_image(inputs_dir, "tower");
    seam::AnimationRecorder recorder(tower.size());
    const CarveResult result = seam::carve_to_size(
        tower, tower.cols * 7 / 10, tower.rows * 85 / 100, CarveOptions(), recorder.callback());
    recorder.add_final_frame(result.image);
    const std::string dir = output_dir + "/tower/";
    save(dir + "dp_both_seams.png", seam::draw_all_seams(tower, result.seams));
    if (!seam::write_video(dir + "dp_steps.mp4", recorder.frames(), 30.0)) {
        std::cout << "visual: video backend not available, writing PNG frames only" << std::endl;
    }
    // A few still frames of the animation for the report.
    const std::vector<cv::Mat>& frames = recorder.frames();
    for (int percent : {0, 25, 50, 75, 100}) {
        const size_t index = (frames.size() - 1) * percent / 100;
        save(dir + "dp_step_" + std::to_string(percent) + ".png", frames[index]);
    }
    std::cout << "visual: animation done (" << frames.size() << " frames)" << std::endl;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 4) {
        std::cerr << "usage: seam_bench size|seams|cost|improve|memory|visual|all <inputs dir> "
                     "<output dir>\n";
        return 1;
    }
    const std::string experiment = argv[1];
    const std::string inputs_dir = argv[2];
    const std::string output_dir = argv[3];
    const bool all = (experiment == "all");

    try {
        bool known = all;
        // The timing experiments are repeated; see kRepetitions.
        for (int repetition = 1; repetition <= kRepetitions; ++repetition) {
            if (all || experiment == "size") {
                run_size_experiment(inputs_dir, output_dir, repetition);
                known = true;
            }
            if (all || experiment == "seams") {
                run_seams_experiment(inputs_dir, output_dir, repetition);
                known = true;
            }
            if (all || experiment == "improve") {
                run_improve_experiment(inputs_dir, output_dir, repetition);
                known = true;
            }
        }
        if (all || experiment == "cost") {
            run_cost_experiment(inputs_dir, output_dir);
            known = true;
        }
        if (all || experiment == "memory") {
            run_memory_experiment(inputs_dir, output_dir);
            known = true;
        }
        if (all || experiment == "visual") {
            run_visual_experiment(inputs_dir, output_dir);
            known = true;
        }
        if (!known) {
            std::cerr << "unknown experiment '" << experiment << "'\n";
            return 1;
        }
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << "\n";
        return 1;
    }
    return 0;
}
