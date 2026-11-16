// visualize.cpp - PLACEHOLDER.
//
// Seam drawing and the step-by-step animation (bonus 7a) are implemented in
// milestone 5 (owner: Yasmin Tazi, branch m5-visual-app).
// Until that milestone is merged, every function below only reports that it is not there
// yet (the command-line options --seams-image, --video, --frames and --energy-image).
#include "seam/visualize.hpp"

namespace seam {

cv::Mat to_bgr(const cv::Mat&) {
    throw NotImplementedYet("seam visualisation (milestone 5, Yasmin Tazi)");
}

cv::Mat draw_seam(const cv::Mat&, const Seam&, Direction) {
    throw NotImplementedYet("seam visualisation (milestone 5, Yasmin Tazi)");
}

cv::Mat draw_all_seams(const cv::Mat&, const std::vector<SeamRecord>&) {
    throw NotImplementedYet("seam visualisation (milestone 5, Yasmin Tazi)");
}

cv::Mat colourise_energy(const EnergyMap&) {
    throw NotImplementedYet("seam visualisation (milestone 5, Yasmin Tazi)");
}

cv::Size animation_canvas_size(cv::Size, int, int) {
    throw NotImplementedYet("seam visualisation (milestone 5, Yasmin Tazi)");
}

AnimationRecorder::AnimationRecorder(cv::Size canvas_size) : canvas_size_(canvas_size) {
    throw NotImplementedYet("seam visualisation (milestone 5, Yasmin Tazi)");
}

void AnimationRecorder::add_frame(const cv::Mat&) {
    throw NotImplementedYet("seam visualisation (milestone 5, Yasmin Tazi)");
}

StepCallback AnimationRecorder::callback() {
    throw NotImplementedYet("seam visualisation (milestone 5, Yasmin Tazi)");
}

void AnimationRecorder::add_final_frame(const cv::Mat&) {
    throw NotImplementedYet("seam visualisation (milestone 5, Yasmin Tazi)");
}

const std::vector<cv::Mat>& AnimationRecorder::frames() const {
    throw NotImplementedYet("seam visualisation (milestone 5, Yasmin Tazi)");
}

bool write_video(const std::string&, const std::vector<cv::Mat>&, double) {
    throw NotImplementedYet("seam visualisation (milestone 5, Yasmin Tazi)");
}

int write_frames(const std::string&, const std::vector<cv::Mat>&, int) {
    throw NotImplementedYet("seam visualisation (milestone 5, Yasmin Tazi)");
}

}  // namespace seam
