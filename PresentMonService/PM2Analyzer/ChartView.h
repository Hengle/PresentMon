#pragma once

#include <QtCharts/QChartView>
#include <QtWidgets/QRubberBand>

#include "FrameGraphics.h"
#define QT_CHARTS_USE_NAMESPACE

class ChartView : public QChartView {
  Q_OBJECT

 public:
  ChartView(QWidget* parent = 0);
  void SetClipTime(ClippingLineGraphicsItem* start_time_item,
                   ClippingLineGraphicsItem* end_time_item);
  qreal GetStartTime();
  qreal GetEndTime();

 protected:
  void mousePressEvent(QMouseEvent* event) Q_DECL_OVERRIDE;
  void mouseMoveEvent(QMouseEvent* event) Q_DECL_OVERRIDE;
  void mouseReleaseEvent(QMouseEvent* event) Q_DECL_OVERRIDE;
  void keyPressEvent(QKeyEvent* event) Q_DECL_OVERRIDE;
  void keyReleaseEvent(QKeyEvent* event) Q_DECL_OVERRIDE;

  void wheelEvent(QWheelEvent* event) Q_DECL_OVERRIDE;

 public slots:
  void Print();
  void onRubberBandActive(QRect rect) {
    rubberband_ptr_->setGeometry(rect);
    rubberband_ptr_->show();
  }
  void ZoomInChart(QRectF bounded_geomotry);
  void ZoomInChart(qreal x_factor, qreal x_center);
  void ZoomChart(qreal x_factor, qreal dx, qreal dy);
  void ZoomReset();
  void ZoomResetSyncSlot();
  void onClipWindowChange(float start, float end) {
    start_time_clip_line_->SetTime(start);
    end_time_clip_line_->SetTime(end);
  };
  void onPlotAreaChange(const QRectF&);
  void ChartScroll(int x, int y);

 signals:
  void ActivateRubberBand(QRect rect);
  void FinishedRubberBand(QRectF bounded_geomotry);
  void ZoomSync(qreal x_factor, qreal x_center);
  void ZoomSyncWheel(qreal x_factor, qreal dx, qreal dy);
  void UpdateClipWindow(float, float);
  void ClipWindowSync(float, float);
  void ZoomResetSync();
  void ZoomComplete(float, float);
  void FpsZoomComplete(float, float);
  void ChartScrollSync(int, int);

 private slots:
  void ShowContextMenu(const QPoint&);

 private:
  void SetPanning(bool panning) { is_panning_ = panning; }
  QRectF GetZoomRect(qreal x_factor, qreal x_center);
  QRect GetBoundedGeometry();
  bool IsClipLineSelected();
  bool IsClipLineHovered();
  void ClearClipLineSelection();
  void UpdateClipLineGeometries() {
    start_time_clip_line_->UpdateGeometry();
    end_time_clip_line_->UpdateGeometry();
  };

  QRubberBand* rubberband_ptr_;
  bool is_panning_;
  bool is_clicking_;
  ClippingLineGraphicsItem* start_time_clip_line_;
  ClippingLineGraphicsItem* end_time_clip_line_;
  float max_time_;
  float min_time_;
  QGraphicsTextItem* clip_window_text_;
  float current_scale_;
  int current_scroll_x_;
  int current_scroll_y_;
  QPoint last_mouse_pos_;
};
