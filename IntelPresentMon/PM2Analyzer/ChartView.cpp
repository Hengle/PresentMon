#include "ChartView.h"

#include <QtWidgets/Qmenu.h>

#include <QLineSeries>
#include <QtGui/QMouseEvent>
#include <QtPrintSupport/QPrintDialog>
#include <QtPrintSupport/QPrinter>
#include <QtWidgets/QApplication>
#include <QtWidgets/QStyleOptionGraphicsItem>
#include <iostream>

#include "MSG.h"

static const qreal kDefaultZoomFactor = 0.8;
static const float kScaleMin = 0.5;
static const float kScaleMax = 1000.0;

ChartView::ChartView(QWidget* parent)
    : QChartView(parent),
      is_clicking_(false),
      start_time_clip_line_(nullptr),
      end_time_clip_line_(nullptr),
      clip_window_text_(nullptr),
      max_time_(0),
      min_time_(0),
      current_scale_(1.0),
      current_scroll_x_(0),
      current_scroll_y_(0),
      is_panning_(false) {
  setDragMode(QGraphicsView::NoDrag);
  setMouseTracking(true);
  setRubberBand(QChartView::HorizontalRubberBand);
  rubberband_ptr_ = this->findChild<QRubberBand*>();
  setRenderHint(QPainter::Antialiasing);

  // set up context menu on right click
  setContextMenuPolicy(Qt::CustomContextMenu);

  connect(this, SIGNAL(customContextMenuRequested(const QPoint&)), this,
          SLOT(ShowContextMenu(const QPoint&)));
}

void ChartView::ShowContextMenu(const QPoint& pos) {
  QMenu contextMenu(tr("Context menu"), this);

  QAction action1("Reset zoom... (Key press \"f\")", this);
  connect(&action1, SIGNAL(triggered()), this, SLOT(ZoomReset()));
  connect(&action1, SIGNAL(triggered()), this, SLOT(ZoomResetSyncSlot()));
  contextMenu.addAction(&action1);

  QAction action2("Print...", this);
  connect(&action2, SIGNAL(triggered()), this, SLOT(Print()));
  contextMenu.addAction(&action2);
  contextMenu.exec(mapToGlobal(pos));
}

void ChartView::Print() {
  std::unique_ptr<QPrinter> printer = std::make_unique<QPrinter>();
  printer->setPageOrientation(QPageLayout::Landscape);
  printer->setResolution(QPrinter::HighResolution);

  std::unique_ptr<QPrintDialog> dialog =
      std::make_unique<QPrintDialog>(printer.get());
  dialog->setWindowTitle("Exporting graphs");
  if (dialog->exec() != QDialog::Accepted) return;
  std::unique_ptr<QPainter> painter = std::make_unique<QPainter>();
  painter->begin(printer.get());
  render(painter.get());

  painter->end();
}

void ChartView::ZoomInChart(QRectF bounded_geometry) {
  QRectF rect = this->chart()->plotArea();
  float x_factor = rect.width() / bounded_geometry.width() * 1.0f;

  if (current_scale_ * x_factor > kScaleMax) {
    x_factor = kScaleMax / current_scale_;
    bounded_geometry.setWidth(rect.width() * current_scale_ / kScaleMax);
    bounded_geometry.setHeight(rect.height() * current_scale_ / kScaleMax);
  }

  current_scale_ *= x_factor;
  this->chart()->zoomIn(bounded_geometry);
  rubberband_ptr_->close();

  if (bounded_geometry.width() > 0) {
    auto max_y = std::numeric_limits<float>::min();
    auto min_y = std::numeric_limits<float>::max();

    // Get left and right bound
    auto const top_left_scene = mapToScene(
        QPoint(static_cast<int>(rect.x()), static_cast<int>(rect.y())));
    auto const top_left_item = chart()->mapFromScene(top_left_scene);
    auto const left_value = chart()->mapToValue(top_left_item);
    auto const bottom_right_scene = mapToScene(QPoint(
        static_cast<int>(rect.x() + rect.width()), static_cast<int>(rect.y())));
    auto const bottom_right_item = chart()->mapFromScene(bottom_right_scene);
    auto const right_value = chart()->mapToValue(bottom_right_item);

    for (auto item : items()) {
      FrameGraphicsItem* gfx_item = dynamic_cast<FrameGraphicsItem*>(item);
      if (gfx_item != nullptr) {
        gfx_item->UpdateWidth(x_factor);
        auto frame = gfx_item->GetFrame();
        float frame_time = frame->time_in_ms_ - frame->first_frame_time_in_ms_;

        if (frame_time >= left_value.x() && frame_time <= right_value.x()) {
          max_y = std::max(max_y, frame->normalized_actual_flip_);
          min_y = std::min(min_y, frame->normalized_app_start_);
        }
        ZoomComplete(min_y, max_y);
      };
    }

    auto series = this->chart()->series();
    for (auto fps_line : series) {
      if (fps_line->name() == QString("FPS")) {
        QLineSeries* fps_line_series = dynamic_cast<QLineSeries*>(fps_line);
        for (QPointF point : fps_line_series->points()) {
          if (point.x() >= left_value.x() && point.x() <= right_value.x()) {
            max_y = std::max(max_y, (float)point.y());
            min_y = std::min(min_y, (float)0.0);
          }
        }
        FpsZoomComplete(0, max_y);
      }
    }

    UpdateClipLineGeometries();
  }
}

QRect ChartView::GetBoundedGeometry() {
  QRectF plotArea = chart()->plotArea();
  qreal xbound = plotArea.x();
  qreal ybound = plotArea.y() + plotArea.height();
  QPointF rb_tl = rubberband_ptr_->geometry().topLeft();
  QPointF rb_br = rubberband_ptr_->geometry().bottomRight();
  QRect bounded_geometry = rubberband_ptr_->geometry();
  if (rb_tl.x() < xbound) {
    bounded_geometry.setX(xbound);
  }
  if (rb_br.y() > ybound) {
    bounded_geometry.setBottom(ybound);
  }

  return bounded_geometry;
}

QRectF ChartView::GetZoomRect(qreal x_factor, qreal x_center) {
  QRectF rect = chart()->plotArea();
  qreal widthOriginal = rect.width();
  qreal heightOriginal = rect.height();
  rect.setWidth(widthOriginal / x_factor);
  qreal centerScale = (x_center / widthOriginal);
  qreal leftOffset = (x_center - (rect.width() * centerScale));

  rect.moveLeft(rect.x() + leftOffset);

  return rect;
}

void ChartView::ZoomInChart(qreal x_factor, qreal x_center) {
  QRectF rect = GetZoomRect(x_factor, x_center);
  ZoomInChart(rect);
}

void ChartView::ZoomChart(qreal x_factor, qreal dx, qreal dy) {
  QRectF rect = chart()->plotArea();
  QRectF zoom_rect =
      GetZoomRect(x_factor, this->chart()->plotArea().x() +
                                this->chart()->plotArea().width() / 2);

  if (current_scale_ * x_factor > kScaleMax) {
    x_factor = kScaleMax / current_scale_;
    zoom_rect.setWidth(rect.width() * current_scale_ / kScaleMax);
    zoom_rect.setHeight(rect.height() * current_scale_ / kScaleMax);
  }

  current_scale_ *= x_factor;
  this->chart()->zoomIn(zoom_rect);

  qreal widthOriginal = rect.width();
  qreal heightOriginal = rect.height();
  rect.setWidth(widthOriginal / x_factor);
  rect.setHeight(heightOriginal / x_factor);
  chart()->scroll(-dx, -dy);

  auto max_y = std::numeric_limits<float>::min();
  auto min_y = std::numeric_limits<float>::max();

  // Get left and right bound
  auto const top_left_scene = mapToScene(
      QPoint(static_cast<int>(rect.x()), static_cast<int>(rect.y())));
  auto const top_left_item = chart()->mapFromScene(top_left_scene);
  auto const left_value = chart()->mapToValue(top_left_item);
  auto const bottom_right_scene = mapToScene(QPoint(
      static_cast<int>(rect.x() + rect.width()), static_cast<int>(rect.y())));
  auto const bottom_right_item = chart()->mapFromScene(bottom_right_scene);
  auto const right_value = chart()->mapToValue(bottom_right_item);

  for (auto item : items()) {
    FrameGraphicsItem* gfx_item = dynamic_cast<FrameGraphicsItem*>(item);
    if (gfx_item != nullptr) {
      gfx_item->UpdateWidth(x_factor);
      auto frame = gfx_item->GetFrame();
      float frame_time = frame->time_in_ms_ - frame->first_frame_time_in_ms_;

      if (frame_time >= left_value.x() && frame_time <= right_value.x()) {
        max_y = std::max(max_y, frame->normalized_actual_flip_);
        min_y = std::min(min_y, frame->normalized_app_start_);
      }
      ZoomComplete(min_y, max_y);
    };
  }

  auto series = this->chart()->series();
  for (auto fps_line : series) {
    if (fps_line->name() == QString("FPS")) {
      QLineSeries* fps_line_series = dynamic_cast<QLineSeries*>(fps_line);
      for (QPointF point : fps_line_series->points()) {
        if (point.x() >= left_value.x() && point.x() <= right_value.x()) {
          max_y = std::max(max_y, (float)point.y());
          min_y = std::min(min_y, (float)0.0);
        }
      }
      FpsZoomComplete(0, max_y);
    }
  }

  UpdateClipLineGeometries();
}

void ChartView::ZoomReset() {
  ChartScroll(-1 * current_scroll_x_, -1 * current_scroll_y_);
  ChartScrollSync(-1 * current_scroll_x_, -1 * current_scroll_y_);
  chart()->zoomReset();
  for (auto item : items()) {
    FrameGraphicsItem* gfx_item = dynamic_cast<FrameGraphicsItem*>(item);
    if (gfx_item != nullptr) {
      gfx_item->zoomReset();
    };
  }
  UpdateClipLineGeometries();
  current_scale_ = 1.0;
}

void ChartView::ZoomResetSyncSlot() { ZoomResetSync(); }

void ChartView::SetClipTime(ClippingLineGraphicsItem* start_time,
                            ClippingLineGraphicsItem* end_time) {
  start_time_clip_line_ = start_time;
  end_time_clip_line_ = end_time;
}

bool ChartView::IsClipLineSelected() {
  return (
      (start_time_clip_line_ != nullptr &&
       start_time_clip_line_->isSelected()) ||
      (end_time_clip_line_ != nullptr && end_time_clip_line_->isSelected()));
}

bool ChartView::IsClipLineHovered() {
  return ((start_time_clip_line_ != nullptr &&
           start_time_clip_line_->IsHovered()) ||
          (end_time_clip_line_ != nullptr && end_time_clip_line_->IsHovered()));
}

void ChartView::ClearClipLineSelection() {
  if (start_time_clip_line_ != nullptr) {
    start_time_clip_line_->setSelected(false);
  }
  if (end_time_clip_line_ != nullptr) {
    end_time_clip_line_->setSelected(false);
  }
}

void ChartView::mousePressEvent(QMouseEvent* event) {
  QChart* chart = this->chart();
  QRectF plotArea = chart->plotArea();

  if (!IsClipLineSelected() &&
      (event->button() == Qt::LeftButton && plotArea.contains(event->pos()))) {
    last_mouse_pos_ = event->pos();
    if (!is_panning_) {
      QRect bounded_geometry = GetBoundedGeometry();
      ActivateRubberBand(bounded_geometry);
    } else {
      rubberband_ptr_->close();
    }
  }

  QChartView::mousePressEvent(event);
}
void ChartView::mouseMoveEvent(QMouseEvent* event) {
  auto const widgetPos = event->position();
  auto const scenePos = mapToScene(
      QPoint(static_cast<int>(widgetPos.x()), static_cast<int>(widgetPos.y())));
  auto const chartItemPos = chart()->mapFromScene(scenePos);
  auto const valueGivenSeries = chart()->mapToValue(chartItemPos);

  QChart* chart = this->chart();

  if (IsClipLineSelected()) {
    float start_time = (start_time_clip_line_ == nullptr)
                           ? 0
                           : start_time_clip_line_->GetTime();
    float end_time =
        (end_time_clip_line_ == nullptr) ? 0 : end_time_clip_line_->GetTime();

    start_time_clip_line_->SetRightBound(end_time);
    end_time_clip_line_->SetLeftBound(start_time);
    UpdateClipWindow(start_time, end_time);
  } else if (event->buttons() == Qt::LeftButton && !IsClipLineHovered()) {
    QPoint dPos = event->pos() - last_mouse_pos_;

    if (is_panning_) {
      rubberband_ptr_->close();
      ChartScroll(-dPos.x(), dPos.y());
      ChartScrollSync(-dPos.x(), dPos.y());

      last_mouse_pos_ = event->pos();

      QApplication::restoreOverrideCursor();
    } else {
      QRect bounded_geometry = GetBoundedGeometry();
      ActivateRubberBand(bounded_geometry);
    }
  }
  event->accept();
  QChartView::mouseMoveEvent(event);
}

void ChartView::mouseReleaseEvent(QMouseEvent* event) {
  QApplication::restoreOverrideCursor();
  QChart* chart = this->chart();
  QRectF plot_area = chart->plotArea();
  if (IsClipLineSelected()) {
    ClearClipLineSelection();
  } else if (event->button() == Qt::LeftButton &&
             plot_area.contains(event->pos()) && !is_panning_) {
    QRect bounded_geometry = GetBoundedGeometry();
    FinishedRubberBand(bounded_geometry);
    ZoomInChart(bounded_geometry);
  } else if (event->button() == Qt::RightButton) {
    rubberband_ptr_->close();
    return;
  } else if (event->button() == Qt::LeftButton) {
    qDebug() << "left release";
    return;
  }
  // any other event
  QChartView::mouseReleaseEvent(event);
  chart->update();
}

void ChartView::ChartScroll(int x, int y) {
  chart()->scroll(x, y);
  current_scroll_x_ += x;
  current_scroll_y_ += y;
  UpdateClipLineGeometries();
}

void ChartView::keyPressEvent(QKeyEvent* event) {
  int scroll_x = 0;
  int scroll_y = 0;
  switch (event->key()) {
    case Qt::Key_Plus:
      chart()->zoomIn();
      break;
    case Qt::Key_Minus:
      chart()->zoomOut();
      break;
    case Qt::Key_Left:
      scroll_x = -10;
      break;
    case Qt::Key_Right:
      scroll_x = 10;
      break;
    case Qt::Key_F:
      ZoomReset();
      ZoomResetSync();
      break;
    case Qt::Key_Control:
      SetPanning(true);
      break;
    default:
      QChartView::keyPressEvent(event);
      break;
  }

  ChartScroll(scroll_x, scroll_y);
  ChartScrollSync(scroll_x, scroll_y);
}

void ChartView::keyReleaseEvent(QKeyEvent* event) {
  switch (event->key()) {
    case Qt::Key_Control:
      SetPanning(false);
      break;
    default:
      QChartView::keyReleaseEvent(event);
      break;
  }
}

void ChartView::wheelEvent(QWheelEvent* event) {
  qreal current_x = event->position().x();
  qreal current_y = event->position().y();

  qreal x_factor =
      event->angleDelta().y() > 0 ? 1 / kDefaultZoomFactor : kDefaultZoomFactor;
  qreal x_center = current_x - chart()->plotArea().x();
  qreal y_factor =
      event->angleDelta().y() > 0 ? 1 / kDefaultZoomFactor : kDefaultZoomFactor;

  if (current_scale_ * x_factor < kScaleMin) {
    x_factor = kScaleMin / current_scale_;
    y_factor = kScaleMin / current_scale_;
  }

  if (current_scale_ * x_factor > kScaleMax) {
    x_factor = kScaleMax / current_scale_;
    y_factor = kScaleMax / current_scale_;
  }

  QRectF zoom_rect = GetZoomRect(x_factor, current_x);
  zoom_rect.setWidth(zoom_rect.width() / x_factor);
  zoom_rect.setHeight(zoom_rect.height() / x_factor);

  current_scale_ *= x_factor;
  QRectF rect = chart()->plotArea();

  qreal widthOriginal = rect.width();
  qreal heightOriginal = rect.height();
  rect.setWidth(widthOriginal / x_factor);
  rect.setHeight(heightOriginal / y_factor);

  this->chart()->zoomIn(zoom_rect);

  qreal centerScale = (x_center / widthOriginal);

  qreal dx = (current_x - rect.left()) * (x_factor - 1);
  qreal dy = (current_y - rect.top()) * (y_factor - 1);

  ChartScroll(-dx, 0);
  event->accept();
  QChartView::wheelEvent(event);

  auto max_y = std::numeric_limits<float>::min();
  auto min_y = std::numeric_limits<float>::max();

  // Get left and right bound
  auto const top_left_scene = mapToScene(
      QPoint(static_cast<int>(rect.x()), static_cast<int>(rect.y())));
  auto const top_left_item = chart()->mapFromScene(top_left_scene);
  auto const left_value = chart()->mapToValue(top_left_item);
  auto const bottom_right_scene = mapToScene(QPoint(
      static_cast<int>(rect.x() + rect.width()), static_cast<int>(rect.y())));
  auto const bottom_right_item = chart()->mapFromScene(bottom_right_scene);
  auto const right_value = chart()->mapToValue(bottom_right_item);

  for (auto item : items()) {
    FrameGraphicsItem* gfx_item = dynamic_cast<FrameGraphicsItem*>(item);
    if (gfx_item != nullptr) {
      gfx_item->UpdateWidth(x_factor);
      auto frame = gfx_item->GetFrame();
      float frame_time = frame->time_in_ms_ - frame->first_frame_time_in_ms_;
      if (frame_time >= left_value.x() && frame_time <= right_value.x()) {
        max_y = std::max(max_y, frame->normalized_actual_flip_);
        min_y = std::min(min_y, frame->normalized_app_start_);
      }

      if (x_factor < 1) {
        ZoomComplete(min_y, max_y);
      }
    };
  }

  auto series = this->chart()->series();
  for (auto fps_line : series) {
    if (fps_line->name() == QString("FPS")) {
      QLineSeries* fps_line_series = dynamic_cast<QLineSeries*>(fps_line);
      for (QPointF point : fps_line_series->points()) {
        if (point.x() >= left_value.x() && point.x() <= right_value.x()) {
          max_y = std::max(max_y, (float)point.y());
          min_y = std::min(min_y, (float)0.0);
        }
      }
      FpsZoomComplete(0, max_y);
    }
  }

  chart()->update();

  UpdateClipLineGeometries();
  ZoomSyncWheel(x_factor, dx, dy);
}

qreal ChartView::GetStartTime() {
  if (start_time_clip_line_ != nullptr) {
    return start_time_clip_line_->GetTime();
  } else {
    return 0;
  }
}
qreal ChartView::GetEndTime() {
  if (end_time_clip_line_ != nullptr) {
    return end_time_clip_line_->GetTime();
  } else {
    return 0;
  }
}

void ChartView::onPlotAreaChange(const QRectF& rect) {
  if (start_time_clip_line_ != nullptr) {
    auto line = start_time_clip_line_->line();
    start_time_clip_line_->setLine(QLineF(
        line.p1(), QPointF(line.p1().x(),
                           line.p1().y() + rect.height() + kClipAnchorSize)));
  }

  if (end_time_clip_line_ != nullptr) {
    auto line = end_time_clip_line_->line();
    end_time_clip_line_->setLine(QLineF(
        line.p1(), QPointF(line.p1().x(),
                           line.p1().y() + rect.height() + kClipAnchorSize)));
  }
  UpdateClipLineGeometries();
}