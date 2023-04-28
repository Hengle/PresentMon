#pragma once

#include <QDateTime>
#include <QDateTimeAxis>
#include <QProgressDialog>
#include <QStackedBarSeries>
#include <QStackedWidget>
#include <QtCharts/QChart>
#include <QtCharts/QScatterSeries>
#include <QtCharts/QSplineSeries>
#include <QtCharts/QValueAxis>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QMainWindow>

#include "FrameDataModel.h"
#include "FrameGraphics.h"
#include "ui_PerfCheck.h"

class DataModel;
class FileManager;

enum class PerfMode { kFrameMode = 0, kTimeMode };
enum class PerfMetrics : int {
  kActualFlip = 0,
  kGpuEnd,
  kDriverEnd,
  kAppStart,
  kAnimationError,
  kTotal,
};

enum class PerfMetricsFps : int {
  kFps = 0,
  kMovingAvgFps,
  kAvgFps,
  k99Fps,
  k95Fps,
  k90Fps,
  kTotal
};

class PerfCheck : public QMainWindow {
  Q_OBJECT

 public:
  PerfCheck(FileManager* main_window, QWidget* parent = Q_NULLPTR);
  ~PerfCheck();
  bool SetDataModels(std::vector<std::shared_ptr<DataModel>>& new_data_model);
  FrameDataModel& GetFrameDataModel() { return frame_data_model_; }
  void WriteSettings();
  void ReadSettings();
  bool BuildBarSeries(std::shared_ptr<DataModel> data_model);
  void Print();

 public slots:
  void CalculateStats(float start_time, float end_time);
 signals:
  void StatsChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight);
  void UpdateStatsSummary();
  void plotAreaChanged(const QRectF);

 protected:
  void closeEvent(QCloseEvent* event) Q_DECL_OVERRIDE;
  void resizeEvent(QResizeEvent* event) Q_DECL_OVERRIDE;

 private:
  void InitializeClipLines(float start, float end);
  void UpdateFpsChart();
  void Cancel() { canceled_ = true; };

  // Caching the parent window's pointer
  FileManager* main_window_;

  // This local copy of refrences to datamodels
  FrameDataModel frame_data_model_;
  PerfMode mode_;

  QCheckBox* avg_fps_checkbox_;
  QCheckBox* fps_99_checkbox_;
  QCheckBox* fps_95_checkbox_;
  QCheckBox* fps_90_checkbox_;
  QCheckBox* moving_avg_fps_checkbox_;
  QCheckBox* fps_checkbox_;
  QCheckBox* animation_error_checkbox_;
  QCheckBox* gpu_time_checkbox_;
  QCheckBox* driver_time_checkbox_;
  QCheckBox* present_api_time_checkbox_;
  QCheckBox* display_queue_time_checkbox_;
  QCheckBox* app_time_checkbox_;
  QGroupBox* cpu_groupbox_;
  QGroupBox* fps_groupbox_;

  Ui::PerfCheckClass ui_;
  QTableView* data_model_view_;

  // Helper stats
  std::vector<float> max_y_values_;
  std::vector<float> min_y_values_;
  std::vector<float> max_fps_values_;
  std::vector<float> min_fps_values_;
  std::vector<int> enabled_metrics_;
  std::vector<int> enabled_metrics_fps_;

  ChartView* time_chart_view_;
  ChartView* fps_chart_view_;
  QChart* time_chart_;
  QChart* fps_chart_;

  ClippingLineGraphicsItem* start_time_clip_line_;
  ClippingLineGraphicsItem* end_time_clip_line_;
  ClippingLineGraphicsItem* start_fps_clip_line_;
  ClippingLineGraphicsItem* end_fps_clip_line_;

  QValueAxis* x_axis_time_;
  QValueAxis* y_axis_time_;
  QValueAxis* y_axis_error_;
  QValueAxis* x_axis_fps_;
  QValueAxis* y_axis_fps_;
  QList<float> flip_time_list_;
  QScatterSeries* actual_flip_series_;

  // Fictious line series for time axis
  QLineSeries* dummy_frame_time_series_;
  QLineSeries* dummy_flip_line_series_;
  QLineSeries* dummy_gpu_time_series_;
  QLineSeries* dummy_driver_time_series_;
  QLineSeries* dummy_present_api_time_series_;
  QLineSeries* dummy_app_time_series_;
  QLineSeries* dummy_display_queue_time_series_;

  QScatterSeries* sim_start_point_series_;
  QLineSeries* fps_line_series_;
  QLineSeries* moving_avg_fps_line_series_;
  QLineSeries* avg_fps_line_series_;
  QLineSeries* percentile_99_fps_line_series_;
  QLineSeries* percentile_95_fps_line_series_;
  QLineSeries* percentile_90_fps_line_series_;
  QLineSeries* animation_error_line_series_;

  // Graphics Scene
  QGraphicsScene main_scene_;
  QGraphicsScene fps_scene_;
  QPointF origin_;

  QList<FrameGraphicsItem*> frame_graphics_list_;

  QGraphicsTextItem* fps_text_;
  QGraphicsLineItem* text_line_;
  QProgressDialog* progress_;

  bool canceled_;

 private slots:
  void HoverFps(QPointF point, float fps);
  void HoverLeftFps();
  void UpdateYAxis();
  void UpdateYAxis(float min, float max);
  void UpdateFpsYAxis(float min, float max);
  void AvgFpsChecked();
  void Fps99Checked();
  void Fps95Checked();
  void Fps90Checked();
  void MovingAvgFpsChecked();
  void FpsChecked();
  void AnimationErrorChecked();
  void GpuTimeChecked();
  void CpuChecked();
  void SingleFpsChecked();
  void DriverTimeChecked();
  void PresentApiChecked();
  void QueueTimeChecked();
  void AppTimeChecked();
  void SyncPlotArea(const QRectF rect);
};
