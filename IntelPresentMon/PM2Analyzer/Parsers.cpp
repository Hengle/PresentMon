#include "Parsers.h"

#include <QDebug>
#include <QMessageBox>
#include <QtCore/QVector>
#include <iostream>
#include <string>

bool FrapsParser::SigMatch(const Row& r) {
  {
    return (std::find(r.begin(), r.end(), "Frame") != r.end() &&
            std::find(r.begin(), r.end(), "Time (ms)") == r.end());
  }
}

bool FrapsParser::ParseRows(const std::string& fileName,
                            const std::vector<Row>& rows,
                            std::vector<std::shared_ptr<Frame>>& frames) {
  // Iterate through all rows
  for (auto& r : rows) {
    std::shared_ptr<Frame> frame = nullptr;

    // Save good frames
    if (FrapsParser::ParseLine(r, frame)) {
      frames.push_back(frame);
    } else {
      return false;
    }
  }

  return true;
}

// Allocates and returns a new Frame
bool FrapsParser::ParseLine(const Row& r, std::shared_ptr<Frame>& frame) {
  // Should always have to elements
  if (r.size() != 2) return false;

  // Get frame number and current time.
  int frame_num = std::stoi(r[0].c_str()) - 1;
  qreal thisTime = std::stof(r[1].c_str());

  // Calc delta to get frametime
  qreal delta = thisTime - parser_last_time_;
  parser_last_time_ = thisTime;

  // First entry just grabs original time
  if (first_pass_) {
    first_pass_ = false;
    return false;
  }

  // Create a new frame if needed
  if (frame == nullptr) {
    frame = std::make_shared<Frame>();
    frame->pass.push_back(std::make_shared<PassData>());
  }

  frame->time_in_ms_ = thisTime;
  frame->frame_num_ = frame_num;
  frame->ms_btw_presents_ = delta;
  frame->pass.push_back(std::make_shared<PassData>());

  return true;
}

bool PresentMonParser::SigMatch(const Row& r) {
  return (std::find(r.begin(), r.end(), "Application") != r.end() &&
          std::find(r.begin(), r.end(), "TimeInSeconds") != r.end() &&
          std::find(r.begin(), r.end(), "msBetweenPresents") != r.end());
}

void PresentMonParser::Cancel() { canceled_ = true; }

bool PresentMonParser::ParseRows(const std::string& fileName,
                                 const std::vector<Row>& rows,
                                 std::vector<std::shared_ptr<Frame>>& frames) {
  QProgressDialog progress(
      "Parsing " + QString::fromStdString(fileName) + " ...", "Cancel", 0,
      rows.size());
  progress.setWindowModality(Qt::WindowModal);
  progress.show();
  progress.raise();
  progress.activateWindow();

  uint64_t first_frame_num = 0;
  float first_frame_time_in_ms = 0;
  // Iterate through all rows
  int i = 0;
  for (auto& r : rows) {
    if (canceled_) return false;
    i++;
    progress.setValue(i);
    std::shared_ptr<Frame> frame = nullptr;

    // Save good frames
    if (PresentMonParser::ParseLine(r, frame)) {
      // Update next frame values
      if (!frames.empty()) {
        frame->previous_frame_ = frames.back().get();
        frame->app_start_ = frame->previous_frame_->time_in_ms_ +
                            frame->previous_frame_->ms_in_present_api_ -
                            frame->time_in_ms_;
        frame->app_simulation_ = frame->app_start_;

        frame->animation_step_ = frame->time_in_ms_ + frame->app_simulation_ -
                                 (frame->previous_frame_->time_in_ms_ +
                                  frame->previous_frame_->app_simulation_);
        frame->flip_step_ = frame->ms_btw_display_change_;
        frame->animation_error_ = frame->animation_step_ - frame->flip_step_;

      } else {
        first_frame_num = frame->frame_num_;
        first_frame_time_in_ms = frame->time_in_ms_;
      }
      frame->SetFirstFrameNum(first_frame_num);
      frame->SetFirstFrameTimeInMs(first_frame_time_in_ms);
      if (!frames.empty()) {
        frame->CalculateStatsFromPreviousFrame(frames.back().get());
      }

      NormalizeMetrics(frame);
      frames.push_back(frame);
    }
  }
  return true;
}

// Allocates and returns a new Frame
// Return false if it's the first frame, return true for the rest
bool PresentMonParser::ParseLine(const Row& r, std::shared_ptr<Frame>& frame) {
  // Get frame number and current time.
  static uint64_t frame_num = 0;

  // First entry Find correct field indexes
  if (first_pass_) {
    int i = 0;
    for (auto& row : r) {
      field_index_[row] = i;
      i++;
    }

    first_pass_ = false;
    return false;
  }

  // Create a new frame if needed
  if (frame == nullptr) {
    frame = std::make_shared<Frame>();
    frame->pass.push_back(std::make_shared<PassData>());
  }

  auto it = field_index_.find("Dropped");
  if (it != field_index_.end()) {
    frame->dropped_ = std::stof(r[field_index_["Dropped"]]);
  }

  // Time of the frame
  it = field_index_.find("TimeInSeconds");
  if (it != field_index_.end()) {
    frame->time_in_ms_ = std::stof(r[field_index_["TimeInSeconds"]]) * 1000.0;
  }

  it = field_index_.find("msInPresentAPI");
  if (it != field_index_.end()) {
    frame->ms_in_present_api_ = std::stof(r[field_index_["msInPresentAPI"]]);
  }

  it = field_index_.find("msBetweenPresents");
  if (it != field_index_.end()) {
    frame->ms_btw_presents_ = std::stof(r[field_index_["msBetweenPresents"]]);
  }

  it = field_index_.find("msUntilDisplayed");
  if (it != field_index_.end()) {
    frame->ms_until_displayed_ = std::stof(r[field_index_["msUntilDisplayed"]]);
  }

  it = field_index_.find("msUntilRenderComplete");
  if (it != field_index_.end()) {
    frame->ms_until_render_complete_ =
        std::stof(r[field_index_["msUntilRenderComplete"]]);
  }

  it = field_index_.find("msBetweenDisplayChange");
  if (it != field_index_.end()) {
    frame->ms_btw_display_change_ =
        std::stof(r[field_index_["msBetweenDisplayChange"]]);
  }

  it = field_index_.find("msGPUActive");
  if (it != field_index_.end()) {
    frame->gpu_time_ms_ = std::stof(r[field_index_["msGPUActive"]]);
  }

  frame->pass[0]->GPUStart = frame->ms_until_render_start_;
  frame->pass[0]->GPUEnd = frame->ms_until_render_complete_;

  frame->frame_num_ = frame_num++;

  frame->actual_flip_time_ms_ = frame->ms_until_displayed_;
  frame->reported_flip_time_ms_ = frame->actual_flip_time_ms_;

  return true;
}

void PresentMonParser::NormalizeMetrics(std::shared_ptr<Frame> frame) {
  if (frame == nullptr || frame->previous_frame_ == nullptr) return;

  frame->normalized_present_start_ =
      frame->time_in_ms_ - (frame->previous_frame_->time_in_ms_ +
                            frame->previous_frame_->actual_flip_time_ms_);

  // Animation Time
  frame->normalized_simulation_start_ =
      frame->normalized_present_start_ + frame->app_simulation_;

  // App time
  frame->normalized_app_start_ =
      frame->normalized_present_start_ + frame->app_start_;
  frame->app_time_ms_ = frame->pass[0]->present - frame->app_start_;

  /// GPU
  frame->normalized_gpu_start_ =
      frame->normalized_present_start_ + frame->pass[0]->GPUStart;
  frame->normalized_gpu_end_ =
      frame->normalized_present_start_ + frame->pass[0]->GPUEnd;
  frame->gpu_time_ms_ =
      frame->ms_until_render_complete_ - frame->ms_in_present_api_;

  // Flip queue time
  frame->normalized_scheduled_flip_ =
      frame->normalized_present_start_ + frame->reported_flip_time_ms_;

  // Actual Flip time
  frame->normalized_actual_flip_ =
      frame->normalized_present_start_ + frame->actual_flip_time_ms_;
}

bool PresentMon2Parser::SigMatch(const Row& r) {
  if (std::find(r.begin(), r.end(), "application") == r.end()) goto mismatch;
  if (std::find(r.begin(), r.end(), "time_in_seconds") == r.end())
    goto mismatch;
  if (std::find(r.begin(), r.end(), "ms_in_present_api") == r.end())
    goto mismatch;
  if (std::find(r.begin(), r.end(), "ms_until_render_complete") == r.end())
    goto mismatch;
  if (std::find(r.begin(), r.end(), "ms_until_render_start") == r.end())
    goto mismatch;
  if (std::find(r.begin(), r.end(), "ms_gpu_active") == r.end()) goto mismatch;
  if (std::find(r.begin(), r.end(), "ms_between_display_change") == r.end())
    goto mismatch;
  if (std::find(r.begin(), r.end(), "ms_until_displayed") == r.end())
    goto mismatch;
  if (std::find(r.begin(), r.end(), "ms_between_presents") == r.end())
    goto mismatch;

  return true;

mismatch:
  return false;
}

void PresentMon2Parser::Cancel() { canceled_ = true; }

bool PresentMon2Parser::ParseRows(const std::string& fileName,
                                  const std::vector<Row>& rows,
                                  std::vector<std::shared_ptr<Frame>>& frames) {
  progress_->reset();
  progress_->setLabelText("Parsing " + QString::fromStdString(fileName) +
                          " ...");
  progress_->setCancelButtonText("Cancel");
  progress_->setMinimum(0);
  progress_->setMaximum(rows.size());
  progress_->setWindowModality(Qt::WindowModal);
  progress_->show();
  progress_->raise();
  progress_->activateWindow();

  uint64_t first_frame_num = 0;
  float first_frame_time_in_ms = 0;
  // Iterate through all rows
  int i = 0;
  for (auto& r : rows) {
    if (canceled_) return false;
    i++;
    progress_->setValue(i);
    std::shared_ptr<Frame> frame = nullptr;

    // Save good frames
    if (PresentMon2Parser::ParseLine(r, frame)) {
      // Update next frame values
      if (!frames.empty()) {
        frame->previous_frame_ = frames.back().get();
        frame->app_start_ = frame->previous_frame_->time_in_ms_ +
                            frame->previous_frame_->ms_in_present_api_ -
                            frame->time_in_ms_;
        frame->app_simulation_ = frame->app_start_;

        frame->animation_step_ = frame->time_in_ms_ + frame->app_simulation_ -
                                 (frame->previous_frame_->time_in_ms_ +
                                  frame->previous_frame_->app_simulation_);
        frame->flip_step_ = frame->ms_btw_display_change_;
        frame->animation_error_ = frame->animation_step_ - frame->flip_step_;

      } else {
        first_frame_num = frame->frame_num_;
        first_frame_time_in_ms = frame->time_in_ms_;
      }
      frame->SetFirstFrameNum(first_frame_num);
      frame->SetFirstFrameTimeInMs(first_frame_time_in_ms);
      if (!frames.empty()) {
        frame->CalculateStatsFromPreviousFrame(frames.back().get());
      }

      NormalizeMetrics(frame);
      frames.push_back(frame);
    }
  }
  return true;
}

// Allocates and returns a new Frame
// Return false if it's the first frame, return true for the rest
bool PresentMon2Parser::ParseLine(const Row& r, std::shared_ptr<Frame>& frame) {
  // Get frame number and current time.
  static int frame_num_ = 0;
  if (r[0] == "") return false;

  // First entry Find correct field indexes
  if (first_pass_) {
    int i = 0;
    for (auto& row : r) {
      field_index_[row] = i;
      i++;
    }

    first_pass_ = false;
    return false;
  }

  // Create a new frame if needed
  if (frame == nullptr) {
    frame = std::make_shared<Frame>();
    frame->pass.push_back(std::make_shared<PassData>());
  }

  auto it = field_index_.find("dropped");
  if (it != field_index_.end()) {
    frame->dropped_ = std::stof(r[field_index_["dropped"]]);
  }

  // application name

  it = field_index_.find("application");
  if (it != field_index_.end()) {
    frame->application_ = r[field_index_["application"]];
  }

  // Time of the frame
  it = field_index_.find("time_in_seconds");
  if (it != field_index_.end()) {
    frame->time_in_ms_ = std::stof(r[field_index_["time_in_seconds"]]) * 1000.0;
  }

  it = field_index_.find("ms_in_present_api");
  if (it != field_index_.end()) {
    frame->ms_in_present_api_ = std::stof(r[field_index_["ms_in_present_api"]]);
  }

  it = field_index_.find("ms_between_presents");
  if (it != field_index_.end()) {
    frame->ms_btw_presents_ = std::stof(r[field_index_["ms_between_presents"]]);
  }

  it = field_index_.find("ms_until_displayed");
  if (it != field_index_.end()) {
    frame->ms_until_displayed_ =
        std::stof(r[field_index_["ms_until_displayed"]]);
  }

  it = field_index_.find("ms_until_render_complete");
  if (it != field_index_.end()) {
    frame->ms_until_render_complete_ =
        std::stof(r[field_index_["ms_until_render_complete"]]);
  }

  it = field_index_.find("ms_until_render_start");
  if (it != field_index_.end()) {
    auto idx = it->second;
    auto render_start = r[idx];
    frame->ms_until_render_start_ = std::stof(render_start);
  }

  it = field_index_.find("ms_between_display_change");
  if (it != field_index_.end()) {
    frame->ms_btw_display_change_ =
        std::stof(r[field_index_["ms_between_display_change"]]);
  }

  it = field_index_.find("ms_gpu_active");
  if (it != field_index_.end()) {
    frame->gpu_time_ms_ = std::stof(r[field_index_["ms_gpu_active"]]);
  }

  frame->pass[0]->GPUStart = frame->ms_until_render_start_;
  frame->pass[0]->GPUEnd = frame->ms_until_render_complete_;

  frame->frame_num_ = frame_num_++;

  frame->actual_flip_time_ms_ = frame->ms_until_displayed_;
  frame->reported_flip_time_ms_ = frame->actual_flip_time_ms_;

  return true;
}

void PresentMon2Parser::NormalizeMetrics(std::shared_ptr<Frame> frame) {
  if (frame == nullptr || frame->previous_frame_ == nullptr) return;

  frame->normalized_present_start_ =
      frame->time_in_ms_ - (frame->previous_frame_->time_in_ms_ +
                            frame->previous_frame_->actual_flip_time_ms_);

  // Animation Time
  frame->normalized_simulation_start_ =
      frame->normalized_present_start_ + frame->app_simulation_;

  // App time
  frame->normalized_app_start_ =
      frame->normalized_present_start_ + frame->app_start_;
  frame->app_time_ms_ = frame->pass[0]->present - frame->app_start_;

  /// GPU
  frame->normalized_gpu_start_ =
      frame->normalized_present_start_ + frame->pass[0]->GPUStart;
  frame->normalized_gpu_end_ =
      frame->normalized_present_start_ + frame->pass[0]->GPUEnd;

  // Flip queue time
  frame->normalized_scheduled_flip_ =
      frame->normalized_present_start_ + frame->reported_flip_time_ms_;

  // Actual Flip time
  frame->normalized_actual_flip_ =
      frame->normalized_present_start_ + frame->actual_flip_time_ms_;
}

bool NewPresentMonParser::SigMatch(const Row& r) {
  return (std::find(r.begin(), r.end(), "Application") != r.end() &&
          std::find(r.begin(), r.end(), "INTC_FrameID") != r.end() &&
          std::find(r.begin(), r.end(), "INTC_ActualFlipTime") != r.end() &&
          std::find(r.begin(), r.end(), "TimeInSeconds") != r.end() &&
          std::find(r.begin(), r.end(), "msBetweenPresents") != r.end());
}

bool NewPresentMonParser::ParseRows(
    const std::string& fileName, const std::vector<Row>& rows,
    std::vector<std::shared_ptr<Frame>>& frames) {
  {
    uint64_t first_frame_num = 0;
    float first_frame_time_in_ms = 0;
    // Iterate through all rows
    for (auto& r : rows) {
      std::shared_ptr<Frame> frame = nullptr;

      // Save good frames
      if (NewPresentMonParser::ParseLine(r, frame)) {
        // Update next frame values
        if (!frames.empty()) {
          frame->previous_frame_ = frames.back().get();
          frame->animation_step_ = frame->time_in_ms_ + frame->app_simulation_ -
                                   (frame->previous_frame_->time_in_ms_ +
                                    frame->previous_frame_->app_simulation_);
          frame->flip_step_ = frame->time_in_ms_ + frame->actual_flip_time_ms_ -
                              (frame->previous_frame_->time_in_ms_ +
                               frame->previous_frame_->actual_flip_time_ms_);
          frame->animation_error_ = frame->animation_step_ - frame->flip_step_;

        } else {
          first_frame_num = frame->frame_num_;
          first_frame_time_in_ms = frame->time_in_ms_;
        }
        frame->SetFirstFrameNum(first_frame_num);
        frame->SetFirstFrameTimeInMs(first_frame_time_in_ms);
        if (!frames.empty()) {
          frame->CalculateStatsFromPreviousFrame(frames.back().get());
        }

        NormalizeMetrics(frame);
        frames.push_back(frame);
      } else {
        // ToDo(jtseng2) Pop up error
      }
    }
    return true;
  }
}

// Allocates and returns a new Frame
bool NewPresentMonParser::ParseLine(const Row& r,
                                    std::shared_ptr<Frame>& frame) {
  // First entry Find correct field indexes
  if (first_pass_) {
    int i = 0;
    for (auto& row : r) {
      field_index_[row] = i;
      i++;
    }

    first_pass_ = false;
    return false;
  }

  // Create a new frame if needed
  if (frame == nullptr) {
    frame = std::make_shared<Frame>();
    frame->pass.push_back(std::make_shared<PassData>());
  }
  // Time of the frame
  auto it = field_index_.find("TimeInSeconds");
  if (it != field_index_.end()) {
    frame->time_in_ms_ = std::stof(r[field_index_["TimeInSeconds"]]) * 1000.0;
  }

  it = field_index_.find("msInPresentAPI");
  if (it != field_index_.end()) {
    frame->ms_in_present_api_ = std::stof(r[field_index_["msInPresentAPI"]]);
  }

  it = field_index_.find("msBetweenPresents");
  if (it != field_index_.end()) {
    frame->ms_btw_presents_ = std::stof(r[field_index_["msBetweenPresents"]]);
  }

  it = field_index_.find("msUntilDisplayed");
  if (it != field_index_.end()) {
    frame->ms_until_displayed_ = std::stof(r[field_index_["msUntilDisplayed"]]);
  }

  it = field_index_.find("msBetweenDisplayChange");
  if (it != field_index_.end()) {
    frame->ms_btw_display_change_ =
        std::stof(r[field_index_["msBetweenDisplayChange"]]);
  }

  it = field_index_.find("INTC_FrameID");
  if (it != field_index_.end()) {
    frame->frame_num_ = std::stof(r[field_index_["INTC_FrameID"]]);
  }

  it = field_index_.find("INTC_AppSimulationTime");
  if (it != field_index_.end()) {
    frame->app_simulation_ =
        std::stof(r[field_index_["INTC_AppSimulationTime"]]);
  }

  it = field_index_.find("INTC_AppWorkStart");
  if (it != field_index_.end()) {
    frame->app_start_ = std::stof(r[field_index_["INTC_AppWorkStart"]]);
  }

  frame->pass[0]->kernelSubmitStart =
      std::stof(r[field_index_["INTC_KernelDriverSubmitStart"]]);
  frame->pass[0]->kernelSubmitEnd =
      std::stof(r[field_index_["INTC_KernelDriverSubmitEnd"]]);
  frame->pass[0]->present = std::stof(r[field_index_["INTC_PresentAPICall"]]);

  frame->pass[0]->driverStart =
      std::stof(r[field_index_["INTC_DriverWorkStart"]]);
  frame->pass[0]->driverEnd = std::stof(r[field_index_["INTC_DriverWorkEnd"]]);
  frame->pass[0]->GPUStart = std::stof(r[field_index_["INTC_GPUStart"]]);
  frame->pass[0]->GPUEnd = std::stof(r[field_index_["INTC_GPUEnd"]]);

  frame->flipScheduled = std::stof(r[field_index_["INTC_ScheduledFlipTime"]]);
  frame->flipReceived = std::stof(r[field_index_["INTC_FlipReceivedTime"]]);
  frame->reported_flip_time_ms_ =
      std::stof(r[field_index_["INTC_FlipReportTime"]]);
  frame->programmed_flip_time_ms_ =
      std::stof(r[field_index_["INTC_FlipProgrammingTime"]]);
  frame->actual_flip_time_ms_ =
      std::stof(r[field_index_["INTC_ActualFlipTime"]]);
  frame->fence_report_ =
      std::stof(r[field_index_["INTC_KernelDriverFenceReport"]]);

  return true;
}

void NewPresentMonParser::NormalizeMetrics(
    std::shared_ptr<Frame>
        frame) {  // Find zero line which is previous flip time
  if (frame == nullptr || frame->previous_frame_ == nullptr) return;

  frame->normalized_present_start_ =
      frame->time_in_ms_ - (frame->previous_frame_->time_in_ms_ +
                            frame->previous_frame_->actual_flip_time_ms_);

  // Animation Time
  frame->normalized_simulation_start_ =
      frame->normalized_present_start_ + frame->app_simulation_;

  // App time
  frame->normalized_app_start_ =
      frame->normalized_present_start_ + frame->app_start_;
  frame->app_time_ms_ = frame->pass[0]->present - frame->app_start_;

  // Driver
  frame->normalized_driver_start_ =
      frame->normalized_present_start_ + frame->pass[0]->driverStart;
  frame->driver_time_ms_ =
      frame->pass[0]->driverEnd - frame->pass[0]->driverStart;

  /// GPU
  frame->normalized_gpu_start_ =
      frame->normalized_present_start_ + frame->pass[0]->GPUStart;
  frame->normalized_gpu_end_ =
      frame->normalized_present_start_ + frame->pass[0]->GPUEnd;
  frame->gpu_time_ms_ = frame->pass[0]->GPUEnd - frame->pass[0]->GPUStart;

  // Flip queue time
  frame->normalized_scheduled_flip_ =
      frame->normalized_present_start_ + frame->reported_flip_time_ms_;
  frame->flip_queue_time_ms_ =
      frame->actual_flip_time_ms_ - frame->reported_flip_time_ms_;

  // Actual Flip time
  frame->normalized_actual_flip_ =
      frame->normalized_present_start_ + frame->actual_flip_time_ms_;
}
