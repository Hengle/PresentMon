#include "Frame.h"

void Frame::CalculateStatsFromPreviousFrame(const Frame* frame) {
  fps_presents_ = 1000 / ms_btw_presents_;
  uint64_t num_samples = frame_num_ - first_frame_num_ + 1;
  if (num_samples == 1) {
    moving_avg_fps_ = fps_presents_;
  } else if (num_samples < moving_avg_sample_size_ &&
             frame_num_ > first_frame_num_) {
    moving_avg_fps_ = frame->moving_avg_fps_ * (num_samples - 1) / num_samples +
                      fps_presents_ / num_samples;
  } else {
    moving_avg_fps_ = fps_presents_ / moving_avg_sample_size_ +
                      (frame->moving_avg_fps_ * (moving_avg_sample_size_ - 1)) /
                          moving_avg_sample_size_;
  }
}