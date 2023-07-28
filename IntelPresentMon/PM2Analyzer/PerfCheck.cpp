#include "PerfCheck.h"

#include <qdebug.h>

#include <QGraphicsSceneHoverEvent>
#include <QMdiSubWindow>
#include <QObject>
#include <QPen>
#include <QSettings>
#include <QTextStream>
#include <QtCharts/QLegendMarker>
#include <QtCharts/QLineSeries>
#include <QtCharts/QVXYModelMapper>
#include <QtCharts/QValueAxis>
#include <QtCore/QtMath>
#include <QtWidgets/QApplication>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QListView>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QStackedLayout>
#include <QtWidgets/QTableView>
#include <iostream>
#include <random>

#include "ColorPalette.h"
#include "DataModel.h"
#include "FileManager.h"
#include "FrameDataModel.h"
#include "Logger.h"
#include "chartview.h"

static const float kDefaultMaxYValue = -500.0;
static const float kYPadding = 10.0;
static const float kMaxY = 500;
static const float kMinY = -500;

PerfCheck::PerfCheck(FileManager* main_window, QWidget* parent)
    : QMainWindow(parent),
      main_window_(main_window),
      mode_(PerfMode::kTimeMode),
      max_y_values_(static_cast<float>(PerfMetrics::kTotal), kDefaultMaxYValue),
      min_y_values_(static_cast<float>(PerfMetrics::kTotal),
                    kDefaultMaxYValue * (-1.0)),
      max_fps_values_(static_cast<float>(PerfMetricsFps::kTotal),
                      kDefaultMaxYValue),
      enabled_metrics_fps_(static_cast<int>(PerfMetricsFps::kTotal), 1),
      enabled_metrics_(static_cast<int>(PerfMetrics::kTotal), 1),
      start_time_clip_line_(nullptr),
      end_time_clip_line_(nullptr),
      start_fps_clip_line_(nullptr),
      end_fps_clip_line_(nullptr),
      fps_text_(nullptr),
      text_line_(nullptr),
      canceled_(false) {
  std::cout << "Created PerfCheck" << std::endl;

  ui_.setupUi(this);
  ReadSettings();

  progress_ = new QProgressDialog();
  QObject::connect(progress_, &QProgressDialog::canceled, this,
                   [this]() { this->Cancel(); });

  // Grab key views from the UI
  data_model_view_ = ui_.centralWidget->findChild<QTableView*>("dataTable");

  // series checkbox
  avg_fps_checkbox_ = ui_.centralWidget->findChild<QCheckBox*>("checkAvgFps");
  fps_99_checkbox_ = ui_.centralWidget->findChild<QCheckBox*>("check99thFps");
  fps_95_checkbox_ = ui_.centralWidget->findChild<QCheckBox*>("check95thFps");
  fps_90_checkbox_ = ui_.centralWidget->findChild<QCheckBox*>("check90thFps");
  moving_avg_fps_checkbox_ =
      ui_.centralWidget->findChild<QCheckBox*>("checkMovingAvgFps");
  fps_checkbox_ = ui_.centralWidget->findChild<QCheckBox*>("checkFps");
  animation_error_checkbox_ =
      ui_.centralWidget->findChild<QCheckBox*>("checkAnimationError");
  gpu_time_checkbox_ = ui_.centralWidget->findChild<QCheckBox*>("checkGpuTime");
  driver_time_checkbox_ =
      ui_.centralWidget->findChild<QCheckBox*>("checkDriverTime");
  present_api_time_checkbox_ =
      ui_.centralWidget->findChild<QCheckBox*>("checkTimeInPresent");
  app_time_checkbox_ = ui_.centralWidget->findChild<QCheckBox*>("checkAppTime");
  display_queue_time_checkbox_ =
      ui_.centralWidget->findChild<QCheckBox*>("checkQueueTime");
  cpu_groupbox_ = ui_.centralWidget->findChild<QGroupBox*>("groupCpu");
  fps_groupbox_ = ui_.centralWidget->findChild<QGroupBox*>("groupFps");

  time_chart_view_ = ui_.centralWidget->findChild<ChartView*>("timechart");
  time_chart_view_->setScene(&main_scene_);

  fps_chart_view_ = ui_.centralWidget->findChild<ChartView*>("fpschart");
  fps_chart_view_->setScene(&fps_scene_);

  this->resize(2400, 1300);
  time_chart_view_->resize(2400, 600);
  fps_chart_view_->resize(2400, 600);

  connect(time_chart_view_, SIGNAL(ActivateRubberBand(QRect)), fps_chart_view_,
          SLOT(onRubberBandActive(QRect)));
  connect(fps_chart_view_, SIGNAL(ActivateRubberBand(QRect)), time_chart_view_,
          SLOT(onRubberBandActive(QRect)));

  connect(time_chart_view_, SIGNAL(FinishedRubberBand(QRectF)), fps_chart_view_,
          SLOT(ZoomInChart(QRectF)));
  connect(fps_chart_view_, SIGNAL(FinishedRubberBand(QRectF)), time_chart_view_,
          SLOT(ZoomInChart(QRectF)));

  connect(time_chart_view_, SIGNAL(ZoomSyncWheel(qreal, qreal, qreal)),
          fps_chart_view_, SLOT(ZoomChart(qreal, qreal, qreal)));
  connect(fps_chart_view_, SIGNAL(ZoomSyncWheel(qreal, qreal, qreal)),
          time_chart_view_, SLOT(ZoomChart(qreal, qreal, qreal)));

  connect(time_chart_view_, SIGNAL(ZoomResetSync()), fps_chart_view_,
          SLOT(ZoomReset()));
  connect(fps_chart_view_, SIGNAL(ZoomResetSync()), time_chart_view_,
          SLOT(ZoomReset()));

  connect(time_chart_view_, SIGNAL(ChartScrollSync(int, int)), fps_chart_view_,
          SLOT(ChartScroll(int, int)));
  connect(fps_chart_view_, SIGNAL(ChartScrollSync(int, int)), time_chart_view_,
          SLOT(ChartScroll(int, int)));

  connect(time_chart_view_, SIGNAL(ZoomComplete(float, float)), this,
          SLOT(UpdateYAxis(float, float)));
  connect(fps_chart_view_, SIGNAL(FpsZoomComplete(float, float)), this,
          SLOT(UpdateFpsYAxis(float, float)));

  // Set up tableView
  data_model_view_->horizontalHeader()->setSectionResizeMode(
      QHeaderView::Stretch);
  data_model_view_->verticalHeader()->setSectionResizeMode(
      QHeaderView::Stretch);

  // (ToDo: jtseng2) Hide the table for now until the data selection is
  // linked to charts
  data_model_view_->hide();

  // Initialize Barchart series
  time_chart_ = new QChart();
  fps_chart_ = new QChart();

  time_chart_->setFlag(QGraphicsItem::ItemClipsToShape, true);
  time_chart_->setFlag(QGraphicsItem::ItemClipsChildrenToShape, true);
  sim_start_point_series_ = new QScatterSeries();
  sim_start_point_series_->setName("Simulation Time");

  fps_line_series_ = new QLineSeries();
  fps_line_series_->setName("FPS");

  avg_fps_line_series_ = new QLineSeries();
  avg_fps_line_series_->setName("Average FPS");
  percentile_99_fps_line_series_ = new QLineSeries();
  percentile_99_fps_line_series_->setName("99th Percentile FPS");
  percentile_95_fps_line_series_ = new QLineSeries();
  percentile_95_fps_line_series_->setName("95th Percentile FPS");
  percentile_90_fps_line_series_ = new QLineSeries();
  percentile_90_fps_line_series_->setName("90th Percentile FPS");
  moving_avg_fps_line_series_ = new QLineSeries();
  moving_avg_fps_line_series_->setName("Moving Average FPS");

  actual_flip_series_ = new QScatterSeries();
  actual_flip_series_->setName("Normalized Actual Flip Time(ms)");

  dummy_flip_line_series_ = new QLineSeries();
  dummy_flip_line_series_->setName("Actual Flip Time(ms)");
  dummy_gpu_time_series_ = new QLineSeries();
  dummy_gpu_time_series_->setName("Gpu Time(ms)");
  dummy_driver_time_series_ = new QLineSeries();
  dummy_driver_time_series_->setName("Driver Time(ms)");
  dummy_present_api_time_series_ = new QLineSeries();
  dummy_present_api_time_series_->setName("Present API Time(ms)");
  dummy_app_time_series_ = new QLineSeries();
  dummy_app_time_series_->setName("App Time(ms)");
  dummy_display_queue_time_series_ = new QLineSeries();
  dummy_display_queue_time_series_->setName("Display Queue Time(ms)");
  dummy_frame_time_series_ = new QLineSeries();
  dummy_frame_time_series_->setName("Total Frame Time(ms)");

  animation_error_line_series_ = new QLineSeries();
  animation_error_line_series_->setName("Animation Errors(ms)");
  animation_error_line_series_->hide();

  connect(avg_fps_checkbox_, SIGNAL(clicked()), this, SLOT(AvgFpsChecked()));
  connect(fps_99_checkbox_, SIGNAL(clicked()), this, SLOT(Fps99Checked()));
  connect(fps_95_checkbox_, SIGNAL(clicked()), this, SLOT(Fps95Checked()));
  connect(fps_90_checkbox_, SIGNAL(clicked()), this, SLOT(Fps90Checked()));
  connect(moving_avg_fps_checkbox_, SIGNAL(clicked()), this,
          SLOT(MovingAvgFpsChecked()));
  connect(fps_checkbox_, SIGNAL(clicked()), this, SLOT(SingleFpsChecked()));
  connect(animation_error_checkbox_, SIGNAL(clicked()), this,
          SLOT(AnimationErrorChecked()));
  connect(gpu_time_checkbox_, SIGNAL(clicked()), this, SLOT(GpuTimeChecked()));
  connect(driver_time_checkbox_, SIGNAL(clicked()), this,
          SLOT(DriverTimeChecked()));
  connect(present_api_time_checkbox_, SIGNAL(clicked()), this,
          SLOT(PresentApiChecked()));
  connect(app_time_checkbox_, SIGNAL(clicked()), this, SLOT(AppTimeChecked()));
  connect(display_queue_time_checkbox_, SIGNAL(clicked()), this,
          SLOT(QueueTimeChecked()));
  connect(cpu_groupbox_, SIGNAL(clicked()), this, SLOT(CpuChecked()));
  connect(fps_groupbox_, SIGNAL(clicked()), this, SLOT(FpsChecked()));
}

bool PerfCheck::SetDataModels(
    std::vector<std::shared_ptr<DataModel>>& newDataModels) {
  frame_data_model_.SetModels(newDataModels);

  for (auto dataModel : newDataModels) {
    data_model_view_->setModel(dataModel.get());

    // Series data - used for graphs
    Series* series = new Series;
    series->setName("FrameTime");
    series->setColor("cyan");

    QVXYModelMapper* mapper = new QVXYModelMapper(this);
    mapper->setXColumn(0);
    mapper->setYColumn(1);
    mapper->setSeries(series);
    mapper->setModel(dataModel.get());
  }
  return false;
}

// Save and restore settings
void PerfCheck::WriteSettings() {
  QSettings settings("Moose Soft", "Clipper");

  settings.beginGroup("MainWindow");
  settings.setValue("size", size());
  settings.setValue("pos", pos());
  settings.endGroup();
}

void PerfCheck::ReadSettings() {
  QSettings settings("Moose Soft", "Clipper");

  settings.beginGroup("MainWindow");
  resize(settings.value("size", QSize(400, 400)).toSize());
  move(settings.value("pos", QPoint(200, 200)).toPoint());
  settings.endGroup();
}

// Slot for Print button
void PerfCheck::Print() {
  time_chart_view_->Print();
  fps_chart_view_->Print();
}

PerfCheck::~PerfCheck() {
  if (fps_text_ != nullptr) {
    delete fps_text_;
  }
  if (text_line_ != nullptr) {
    delete text_line_;
  }
  if (progress_ != nullptr) {
    delete progress_;
  }
  std::cout << "Destroyed PerfCheck" << std::endl;
}

void PerfCheck::closeEvent(QCloseEvent* event) {
  WriteSettings();
  // Delete table view
  for (auto data_model : frame_data_model_.data_models_) {
    main_window_->RemoveFile(data_model->fname_.toStdString());
  }
  event->accept();
}

void PerfCheck::resizeEvent(QResizeEvent* event) {
  fps_chart_->setPlotArea(time_chart_->plotArea());
  plotAreaChanged(time_chart_->plotArea());
  plotAreaChanged(time_chart_->plotArea());
  QMainWindow::resizeEvent(event);
}

bool PerfCheck::BuildBarSeries(std::shared_ptr<DataModel> data_model) {
  bool result = true;
  auto frames = data_model->frames();
  auto iter = frames.begin();

  int total_frames = frames.size() - 1;
  int first_frame_num = frames.front()->frame_num_;
  float first_frame_flip = frames.front()->normalized_actual_flip_;
  float x_coord = 0;
  float x_coord_frame = 1;

  QDateTime from = QDateTime::currentDateTime();
  QSize size = time_chart_view_->size();
  int viewport_width = size.width();
  float gfx_item_width = viewport_width * 0.9 / total_frames;

  progress_->reset();
  progress_->setLabelText("Plotting data ...");
  progress_->setCancelButtonText("Cancel");
  progress_->setMinimum(0);
  progress_->setMaximum(total_frames - 1);
  progress_->setWindowModality(Qt::WindowModal);
  progress_->show();
  progress_->raise();
  progress_->activateWindow();

  while (iter != frames.end() - 1) {
    if (canceled_) return false;
    // skip first frame due to no prior frame info
    iter++;
    progress_->setValue(x_coord_frame + 1);
    std::shared_ptr frame = *iter;

    if (frame->dropped_) continue;

    sim_start_point_series_->append(
        QPointF(x_coord, frame->normalized_simulation_start_));

    min_y_values_[static_cast<float>(PerfMetrics::kAppStart)] =
        std::min(min_y_values_[static_cast<float>(PerfMetrics::kAppStart)],
                 frame->normalized_simulation_start_);

    flip_time_list_ << frame->normalized_actual_flip_;
    actual_flip_series_->append(x_coord, frame->normalized_actual_flip_);
    max_y_values_[static_cast<float>(PerfMetrics::kActualFlip)] =
        std::max(max_y_values_[static_cast<float>(PerfMetrics::kActualFlip)],
                 frame->normalized_actual_flip_);
    dummy_flip_line_series_->append(QPointF(x_coord, 0));

    FrameGraphicsItem* item = new FrameGraphicsItem(time_chart_);
    item->Initialize(frame.get(),
                     QPointF(x_coord, frame->normalized_actual_flip_),
                     gfx_item_width);
    frame_graphics_list_.append(item);
    connect(item, SIGNAL(hoverEntered(QPointF, float)), this,
            SLOT(HoverFps(QPointF, float)));
    connect(item, SIGNAL(hoverLeft()), this, SLOT(HoverLeftFps()));

    animation_error_line_series_->append(
        QPointF(x_coord, frame->animation_error_));
    max_y_values_[static_cast<float>(PerfMetrics::kAnimationError)] = std::max(
        max_y_values_[static_cast<float>(PerfMetrics::kAnimationError)],
        frame->animation_error_);
    min_y_values_[static_cast<float>(PerfMetrics::kAnimationError)] = std::min(
        min_y_values_[static_cast<float>(PerfMetrics::kAnimationError)],
        frame->animation_error_);
    avg_fps_line_series_->append(QPointF(x_coord, data_model->stats.avg));
    percentile_99_fps_line_series_->append(
        QPointF(x_coord, data_model->stats._99thpercentile));
    percentile_95_fps_line_series_->append(
        QPointF(x_coord, data_model->stats._95thpercentile));
    percentile_90_fps_line_series_->append(
        QPointF(x_coord, data_model->stats._90thpercentile));
    moving_avg_fps_line_series_->append(
        QPointF(x_coord, frame->moving_avg_fps_));
    max_fps_values_[static_cast<float>(PerfMetricsFps::kMovingAvgFps)] =
        std::max(
            max_fps_values_[static_cast<float>(PerfMetricsFps::kMovingAvgFps)],
            frame->moving_avg_fps_);
    fps_line_series_->append(QPointF(x_coord, frame->fps_presents_));
    max_fps_values_[static_cast<float>(PerfMetricsFps::kFps)] =
        std::max(max_fps_values_[static_cast<float>(PerfMetricsFps::kFps)],
                 frame->fps_presents_);

    x_coord_frame = (frame->frame_num_ - first_frame_num);
    x_coord += frame->ms_btw_display_change_;
  }

  max_fps_values_[static_cast<float>(PerfMetricsFps::kAvgFps)] =
      data_model->stats.avg;
  max_fps_values_[static_cast<float>(PerfMetricsFps::k99Fps)] =
      data_model->stats._99thpercentile;
  max_fps_values_[static_cast<float>(PerfMetricsFps::k95Fps)] =
      data_model->stats._95thpercentile;
  max_fps_values_[static_cast<float>(PerfMetricsFps::k90Fps)] =
      data_model->stats._90thpercentile;

  dummy_flip_line_series_->setVisible(false);
  dummy_gpu_time_series_->setColor(kGpuTimeColor);
  dummy_driver_time_series_->setColor(kDriverTimeColor);
  // (ToDo:jtseng2) Disable until enabled
  dummy_driver_time_series_->hide();
  dummy_present_api_time_series_->setColor(kPresentApiTimeColor);
  dummy_app_time_series_->setColor(kAppTimeColor);
  // (ToDo:jtseng2) Disable until enabled
  dummy_display_queue_time_series_->setColor(kDisplayQueueTimeColor);
  dummy_display_queue_time_series_->hide();
  dummy_frame_time_series_->setColor(kFrameTimeColor);

  sim_start_point_series_->setColor(kSimStartColor);
  sim_start_point_series_->setMarkerShape(QScatterSeries::MarkerShapeRectangle);
  sim_start_point_series_->setMarkerSize(2);
  sim_start_point_series_->setBorderColor(Qt::transparent);

  x_axis_time_ = new QValueAxis();
  x_axis_fps_ = new QValueAxis();

  // Time Chart
  time_chart_->setTheme(QChart::ChartThemeLight);
  time_chart_->setBackgroundVisible(true);
  y_axis_time_ = new QValueAxis();
  y_axis_time_->setLabelFormat("%.2f");
  y_axis_time_->setTitleText("Time(ms)");
  y_axis_time_->setTickAnchor(0);
  y_axis_time_->setTickCount(8);

  // Set FPS
  fps_chart_->setTheme(QChart::ChartThemeLight);
  fps_chart_->setBackgroundVisible(true);
  y_axis_fps_ = new QValueAxis();
  y_axis_fps_->setLabelFormat("%.2f");
  y_axis_fps_->setTitleText("FPS");
  y_axis_fps_->setMin(-10);

  // Secondary y axis for animation errors
  y_axis_error_ = new QValueAxis();
  y_axis_error_->setLabelFormat("%.2f");
  y_axis_error_->setTitleText("Animation Error(ms)");
  y_axis_error_->setTickAnchor(0);
  y_axis_error_->setTickCount(8);

  y_axis_time_->setGridLineVisible(true);
  y_axis_error_->setGridLineVisible(true);
  y_axis_fps_->setGridLineVisible(true);

  time_chart_->addAxis(x_axis_time_, Qt::AlignBottom);
  time_chart_->addAxis(y_axis_time_, Qt::AlignLeft);
  time_chart_->addAxis(y_axis_error_, Qt::AlignRight);
  fps_chart_->addAxis(x_axis_fps_, Qt::AlignBottom);
  fps_chart_->addAxis(y_axis_fps_, Qt::AlignLeft);

  time_chart_->addSeries(actual_flip_series_);
  time_chart_->addSeries(animation_error_line_series_);
  time_chart_->addSeries(dummy_flip_line_series_);
  time_chart_->addSeries(dummy_gpu_time_series_);
  time_chart_->addSeries(dummy_driver_time_series_);
  time_chart_->addSeries(dummy_present_api_time_series_);
  time_chart_->addSeries(dummy_app_time_series_);
  time_chart_->addSeries(dummy_display_queue_time_series_);
  time_chart_->addSeries(dummy_frame_time_series_);
  time_chart_->addSeries(sim_start_point_series_);
  time_chart_->legend()->setAlignment(Qt::AlignTop);

  fps_chart_->addSeries(fps_line_series_);
  fps_chart_->addSeries(avg_fps_line_series_);
  fps_chart_->addSeries(percentile_99_fps_line_series_);
  fps_chart_->addSeries(percentile_95_fps_line_series_);
  fps_chart_->addSeries(percentile_90_fps_line_series_);
  fps_chart_->addSeries(moving_avg_fps_line_series_);
  fps_chart_->legend()->setAlignment(Qt::AlignTop);

  animation_error_line_series_->attachAxis(x_axis_time_);
  animation_error_line_series_->attachAxis(y_axis_error_);
  animation_error_line_series_->setColor(kAnimationErrorColor);
  dummy_flip_line_series_->attachAxis(x_axis_time_);
  dummy_flip_line_series_->attachAxis(y_axis_time_);
  actual_flip_series_->setPointsVisible(true);
  actual_flip_series_->setPointLabelsVisible(false);
  actual_flip_series_->attachAxis(x_axis_time_);
  actual_flip_series_->attachAxis(y_axis_time_);
  sim_start_point_series_->attachAxis(x_axis_time_);
  sim_start_point_series_->attachAxis(y_axis_time_);

  fps_line_series_->attachAxis(x_axis_fps_);
  fps_line_series_->attachAxis(y_axis_fps_);
  avg_fps_line_series_->attachAxis(x_axis_fps_);
  avg_fps_line_series_->attachAxis(y_axis_fps_);
  percentile_99_fps_line_series_->attachAxis(x_axis_fps_);
  percentile_99_fps_line_series_->attachAxis(y_axis_fps_);
  percentile_95_fps_line_series_->attachAxis(x_axis_fps_);
  percentile_95_fps_line_series_->attachAxis(y_axis_fps_);
  percentile_90_fps_line_series_->attachAxis(x_axis_fps_);
  percentile_90_fps_line_series_->attachAxis(y_axis_fps_);
  moving_avg_fps_line_series_->attachAxis(x_axis_fps_);
  moving_avg_fps_line_series_->attachAxis(y_axis_fps_);
  QPen pen(kAvgFpsColor);
  pen.setStyle(Qt::DotLine);
  pen.setWidth(3);
  avg_fps_line_series_->setPen(pen);
  pen.setColor(k99FpsColor);
  percentile_99_fps_line_series_->setPen(pen);
  pen.setColor(k95FpsColor);
  percentile_95_fps_line_series_->setPen(pen);
  pen.setColor(k90FpsColor);
  percentile_90_fps_line_series_->setPen(pen);
  fps_line_series_->setColor(kFpsColor);
  moving_avg_fps_line_series_->setColor(kMovingAvgFpsColor);

  UpdateYAxis();

  x_axis_time_->setRange(0, x_coord);
  x_axis_time_->setTickCount(10);
  x_axis_time_->setLabelFormat("%.2f");
  x_axis_time_->setTitleText("Time(ms)");
  x_axis_time_->setTickInterval(10);
  x_axis_time_->setVisible(true);

  x_axis_fps_->setRange(0, x_coord);
  x_axis_fps_->setTickCount(10);
  x_axis_fps_->setLabelFormat("%.2f");
  x_axis_fps_->setTitleText("Time(ms)");
  x_axis_fps_->setTickInterval(10);
  x_axis_fps_->setVisible(true);

  actual_flip_series_->setMarkerShape(QScatterSeries::MarkerShapeRectangle);
  actual_flip_series_->setColor(kActualFlipColor);
  actual_flip_series_->setMarkerSize(2);

  // Add legends for graphics items
  time_chart_->legend()->setInteractive(true);

  time_chart_view_->setChart(time_chart_);
  fps_chart_view_->setChart(fps_chart_);
  fps_chart_->setPlotArea(time_chart_->plotArea());

  // Add start and end clipping lines
  QLineF line(time_chart_->plotArea().x(),
              time_chart_->plotArea().y() - kClipAnchorSize,
              time_chart_->plotArea().x(),
              time_chart_->plotArea().y() + time_chart_->plotArea().height() +
                  kClipAnchorSize);
  if (start_time_clip_line_ == nullptr) {
    start_time_clip_line_ =
        new ClippingLineGraphicsItem(line, 0, x_coord, time_chart_);
    start_time_clip_line_->SetTime(0);
  }

  if (start_fps_clip_line_ == nullptr) {
    start_fps_clip_line_ =
        new ClippingLineGraphicsItem(line, 0, x_coord, fps_chart_);
    start_fps_clip_line_->SetTime(0);
  }

  if (end_time_clip_line_ == nullptr) {
    end_time_clip_line_ =
        new ClippingLineGraphicsItem(line, 0, x_coord, time_chart_);
    end_time_clip_line_->SetTime(x_coord);
  }

  if (end_fps_clip_line_ == nullptr) {
    end_fps_clip_line_ =
        new ClippingLineGraphicsItem(line, 0, x_coord, fps_chart_);
    end_fps_clip_line_->SetTime(x_coord);
  }

  time_chart_view_->SetClipTime(start_time_clip_line_, end_time_clip_line_);
  fps_chart_view_->SetClipTime(start_fps_clip_line_, end_fps_clip_line_);

  connect(start_time_clip_line_, SIGNAL(TimeChanged(float)),
          start_fps_clip_line_, SLOT(SetTime(float)));
  connect(start_fps_clip_line_, SIGNAL(TimeChanged(float)),
          start_time_clip_line_, SLOT(SetTime(float)));
  connect(end_time_clip_line_, SIGNAL(TimeChanged(float)), end_fps_clip_line_,
          SLOT(SetTime(float)));
  connect(end_fps_clip_line_, SIGNAL(TimeChanged(float)), end_time_clip_line_,
          SLOT(SetTime(float)));
  connect(time_chart_view_, SIGNAL(UpdateClipWindow(float, float)), this,
          SLOT(CalculateStats(float, float)));
  connect(fps_chart_view_, SIGNAL(UpdateClipWindow(float, float)), this,
          SLOT(CalculateStats(float, float)));
  connect(time_chart_, SIGNAL(plotAreaChanged(const QRectF)), time_chart_view_,
          SLOT(onPlotAreaChange(const QRectF)));
  connect(fps_chart_, SIGNAL(plotAreaChanged(const QRectF)), fps_chart_view_,
          SLOT(onPlotAreaChange(const QRectF)));
  connect(time_chart_, SIGNAL(plotAreaChanged(const QRectF)), this,
          SLOT(SyncPlotArea(const QRectF)));

  main_scene_.addItem(time_chart_);
  fps_scene_.addItem(fps_chart_);
  main_scene_.update(main_scene_.sceneRect());
  fps_scene_.update(fps_scene_.sceneRect());

  return result;
}

void PerfCheck::SyncPlotArea(const QRectF rect) {
  fps_chart_->setPlotArea(rect);
}

void PerfCheck::CalculateStats(float start_time, float end_time) {
  for (auto data_model : frame_data_model_.data_models_) {
    data_model->SetStatsWindw(start_time, end_time);
    data_model->calcStats();
    emit data_model->dataChanged(data_model->index(2, 0),
                                 data_model->index(5, 0));
    StatsChanged(data_model->index(2, 0), data_model->index(5, 0));
    UpdateFpsChart();
    UpdateStatsSummary();
  }
}

void PerfCheck::UpdateFpsChart() {
  auto data_model = frame_data_model_.data_models_[0];
  QList<QPointF> new_points;
  for (auto p : avg_fps_line_series_->points()) {
    new_points.append(QPointF(p.x(), data_model->stats.avg));
  }
  avg_fps_line_series_->replace(new_points);

  new_points.clear();
  for (auto p : percentile_99_fps_line_series_->points()) {
    new_points.append(QPointF(p.x(), data_model->stats._99thpercentile));
  }
  percentile_99_fps_line_series_->replace(new_points);

  new_points.clear();
  for (auto p : percentile_95_fps_line_series_->points()) {
    new_points.append(QPointF(p.x(), data_model->stats._95thpercentile));
  }
  percentile_95_fps_line_series_->replace(new_points);

  new_points.clear();
  for (auto p : percentile_90_fps_line_series_->points()) {
    new_points.append(QPointF(p.x(), data_model->stats._90thpercentile));
  }
  percentile_90_fps_line_series_->replace(new_points);

  UpdateYAxis();
}

void PerfCheck::Fps99Checked() {
  bool checked = fps_99_checkbox_->isChecked();
  enabled_metrics_fps_[static_cast<float>(PerfMetricsFps::k99Fps)] = checked;
  if (checked) {
    percentile_99_fps_line_series_->show();
  } else {
    percentile_99_fps_line_series_->hide();
  }
  UpdateYAxis();
}
void PerfCheck::Fps95Checked() {
  bool checked = fps_95_checkbox_->isChecked();
  enabled_metrics_fps_[static_cast<float>(PerfMetricsFps::k95Fps)] = checked;
  if (checked) {
    percentile_95_fps_line_series_->show();
  } else {
    percentile_95_fps_line_series_->hide();
  }
  UpdateYAxis();
}
void PerfCheck::Fps90Checked() {
  bool checked = fps_90_checkbox_->isChecked();
  enabled_metrics_fps_[static_cast<float>(PerfMetricsFps::k90Fps)] = checked;
  if (checked) {
    percentile_90_fps_line_series_->show();
  } else {
    percentile_90_fps_line_series_->hide();
  }
  UpdateYAxis();
}

void PerfCheck::HoverFps(QPointF point, float fps) {
  if (fps_text_ == nullptr) {
    fps_text_ = new QGraphicsTextItem(fps_chart_);
    fps_text_->setZValue(11);
    fps_text_->setParentItem(nullptr);
  }

  fps_text_->setHtml(
      "<body style=\"background-color: #FFFFE0\"> Instant FPS: " +
      QString::number(fps) + "</body>");

  QPointF text_pos = point;

  if (!text_line_) {
    text_line_ = new QGraphicsLineItem(fps_chart_);
    text_line_->setLine(0, 0, 0, fps_chart_view_->height());
    text_line_->setParentItem(nullptr);
    text_line_->setZValue(11);
  }

  text_line_->setPos(text_pos.x(), 0);
  text_line_->setVisible(true);
  fps_text_->setPos(text_pos.x(), fps_chart_->plotArea().height() * 0.2);
  fps_text_->setVisible(true);
}

void PerfCheck::HoverLeftFps() {
  if (text_line_) {
    text_line_->setVisible(false);
  }
  if (fps_text_) {
    fps_text_->setVisible(false);
  }
}

void PerfCheck::AvgFpsChecked() {
  bool checked = avg_fps_checkbox_->isChecked();
  enabled_metrics_fps_[static_cast<float>(PerfMetricsFps::kAvgFps)] = checked;
  if (checked) {
    avg_fps_line_series_->show();
  } else {
    avg_fps_line_series_->hide();
  }
  UpdateYAxis();
}
void PerfCheck::MovingAvgFpsChecked() {
  bool checked = moving_avg_fps_checkbox_->isChecked();
  enabled_metrics_fps_[static_cast<float>(PerfMetricsFps::kMovingAvgFps)] =
      checked;
  if (checked) {
    moving_avg_fps_line_series_->show();
  } else {
    moving_avg_fps_line_series_->hide();
  }
  UpdateYAxis();
}

void PerfCheck::UpdateYAxis() {
  float max_y = kDefaultMaxYValue;
  float min_y = (-1.0) * kDefaultMaxYValue;
  for (int i = 0; i < max_y_values_.size(); i++) {
    max_y = std::max(max_y, max_y_values_[i] * enabled_metrics_[i]);
    min_y = std::min(min_y, min_y_values_[i] * enabled_metrics_[i]);
  }
  y_axis_time_->setMax(std::min(kMaxY, max_y + kYPadding));
  y_axis_time_->setMin(std::max(kMinY, min_y - kYPadding));

  float max_fps = kDefaultMaxYValue;
  for (int i = 0; i < max_fps_values_.size(); i++) {
    max_fps = std::max(max_fps, max_fps_values_[i] * enabled_metrics_fps_[i]);
  }
  y_axis_fps_->setMax(max_fps + kYPadding);
}

void PerfCheck::UpdateYAxis(float min, float max) {
  y_axis_time_->setMax(std::min(kMaxY, max));
  y_axis_time_->setMin(std::max(kMinY, min));
}

void PerfCheck::UpdateFpsYAxis(float min, float max) {
  y_axis_fps_->setMax(std::min(kMaxY, max));
  y_axis_fps_->setMin(std::max(kMinY, min));
}

void PerfCheck::SingleFpsChecked() {
  bool checked = fps_checkbox_->isChecked();
  enabled_metrics_fps_[static_cast<float>(PerfMetricsFps::kFps)] = checked;
  if (checked) {
    fps_line_series_->show();

  } else {
    fps_line_series_->hide();
  }

  UpdateYAxis();
}
void PerfCheck::AnimationErrorChecked() {
  bool checked = animation_error_checkbox_->isChecked();
  if (checked) {
    animation_error_line_series_->show();
  } else {
    animation_error_line_series_->hide();
  }
}

void PerfCheck::GpuTimeChecked() {
  bool checked = gpu_time_checkbox_->isChecked();
  if (checked) {
    dummy_gpu_time_series_->show();
  } else {
    dummy_gpu_time_series_->hide();
  }
  for (auto item : frame_graphics_list_) {
    item->ShowGpuTime(checked);
  }
  main_scene_.update(main_scene_.sceneRect());
}

void PerfCheck::CpuChecked() {
  bool checked = cpu_groupbox_->isChecked();
  present_api_time_checkbox_->setChecked(checked);
  app_time_checkbox_->setChecked(checked);

#if DRIVER_QUEUE_TIME_ENABLED  // (ToDo: jtseng2) Renable driver and display
                               // queue time until counter supported
  driver_time_checkbox_->setChecked(checked);
  display_queue_time_checkbox_->setChecked(checked);
  DriverTimeChecked();
  QueueTimeChecked();
#endif
  PresentApiChecked();
  AppTimeChecked();
}

void PerfCheck::FpsChecked() {
  bool checked = fps_groupbox_->isChecked();
  fps_90_checkbox_->setChecked(checked);
  fps_95_checkbox_->setChecked(checked);
  fps_99_checkbox_->setChecked(checked);
  avg_fps_checkbox_->setChecked(checked);
  moving_avg_fps_checkbox_->setChecked(checked);
  fps_checkbox_->setChecked(checked);
  MovingAvgFpsChecked();
  AvgFpsChecked();
  SingleFpsChecked();
  Fps99Checked();
  Fps95Checked();
  Fps90Checked();
}

void PerfCheck::DriverTimeChecked() {
  bool checked = driver_time_checkbox_->isChecked();
  if (checked) {
    dummy_driver_time_series_->show();
  } else {
    dummy_driver_time_series_->hide();
  }
  for (auto item : frame_graphics_list_) {
    item->ShowDriverTime(checked);
  }
  main_scene_.update(main_scene_.sceneRect());
}

void PerfCheck::PresentApiChecked() {
  bool checked = present_api_time_checkbox_->isChecked();
  if (checked) {
    dummy_present_api_time_series_->show();
  } else {
    dummy_present_api_time_series_->hide();
  }
  for (auto item : frame_graphics_list_) {
    item->ShowPresentApiTime(checked);
  }
  main_scene_.update(main_scene_.sceneRect());
}

void PerfCheck::AppTimeChecked() {
  bool checked = app_time_checkbox_->isChecked();
  if (checked) {
    dummy_app_time_series_->show();
  } else {
    dummy_app_time_series_->hide();
  }
  for (auto item : frame_graphics_list_) {
    item->ShowAppTime(checked);
  }
  main_scene_.update(main_scene_.sceneRect());
}

void PerfCheck::QueueTimeChecked() {
  bool checked = display_queue_time_checkbox_->isChecked();
  if (checked) {
    dummy_display_queue_time_series_->show();
  } else {
    dummy_display_queue_time_series_->hide();
  }
  for (auto item : frame_graphics_list_) {
    item->ShowDisplayQueueTime(checked);
  }
  main_scene_.update(main_scene_.sceneRect());
}