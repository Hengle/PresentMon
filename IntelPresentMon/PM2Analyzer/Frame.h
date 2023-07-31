#pragma once
#include <memory>
#include <string>
#include <vector>

using std::string;

static const int kMovingAvgSampleSize = 10;

class PassData {
 public:
  PassData()
      : kernelSubmitStart(0),
        kernelSubmitEnd(0),
        present(0),
        driverEnd(0),
        driverStart(0),
        GPUEnd(0),
        GPUStart(0){};
  float kernelSubmitStart;
  float kernelSubmitEnd;
  float present;

  float driverStart;
  float driverEnd;

  // GPU times
  float GPUStart;
  float GPUEnd;
};

class Frame {
 public:
  Frame()
      : frame_num_(0),
        first_frame_num_(0),
        time_in_ms_(0),
        fps_presents_(0),
        ms_btw_presents_(0),
        ms_btw_display_change_(0),
        ms_until_displayed_(0),
        ms_until_render_start_(0),
        ms_until_render_complete_(0),
        ms_in_present_api_(0),
        app_start_(0),
        app_simulation_(0),
        fence_report_(0),
        flipScheduled(0),
        flipReceived(0),
        first_frame_time_in_ms_(0),
        reported_flip_time_ms_(0),
        programmed_flip_time_ms_(0),
        actual_flip_time_ms_(0),
        animation_step_(0),
        flip_step_(0),
        animation_error_(0),
        animation_timestamp_ms(0),
        gpu_time_ms_(0),
        app_time_ms_(0),
        driver_time_ms_(0),
        flip_queue_time_ms_(0),
        normalized_gpu_start_(0),
        normalized_gpu_end_(0),
        normalized_app_start_(0),
        normalized_simulation_start_(0),
        normalized_driver_start_(0),
        normalized_present_start_(0),
        normalized_scheduled_flip_(0),
        normalized_actual_flip_(0),
        moving_avg_sample_size_(kMovingAvgSampleSize),
        moving_avg_fps_(0),
        previous_frame_(nullptr),
        dropped_(false){};
  ~Frame(){};
  void CalculateStatsFromPreviousFrame(const Frame* frame);
  void SetFirstFrameNum(uint64_t num) { first_frame_num_ = num; };
  void SetFirstFrameTimeInMs(float time) { first_frame_time_in_ms_ = time; };

  Frame* previous_frame_;

  // Per frame details
  std::string application_;
  uint64_t frame_num_;
  bool dropped_;
  uint64_t first_frame_num_;
  float first_frame_time_in_ms_;
  float time_in_ms_;
  float ms_btw_presents_;
  float ms_until_displayed_;
  float ms_btw_display_change_;
  float ms_until_render_start_;
  float ms_until_render_complete_;
  float ms_in_present_api_;

  // App side markers
  float app_start_;
  float app_simulation_;
  float fence_report_;

  // Multi- Pass rendering
  std::vector<std::shared_ptr<PassData>> pass;

  // Display flip time
  float flipScheduled;
  float flipReceived;
  float reported_flip_time_ms_;
  float programmed_flip_time_ms_;
  float actual_flip_time_ms_;

  // Some calculated stats
  // delta of
  float animation_step_;
  float flip_step_;
  float animation_error_;

  // Stats to be shown on bar graph
  // Relative to previous frame flip time
  float animation_timestamp_ms;

  // moving avg stats
  float fps_presents_;
  int moving_avg_sample_size_;
  float moving_avg_fps_;

  float gpu_time_ms_;
  float app_time_ms_;
  float driver_time_ms_;
  float flip_queue_time_ms_;

  // start time normalized to previous flip
  float normalized_gpu_start_;
  float normalized_gpu_end_;
  float normalized_app_start_;
  float normalized_simulation_start_;
  float normalized_driver_start_;
  float normalized_present_start_;
  float normalized_scheduled_flip_;
  float normalized_actual_flip_;
};