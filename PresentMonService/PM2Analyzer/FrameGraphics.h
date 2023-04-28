#pragma once

#include <QObject>
#include <QtCharts/QChart>
#include <QtGui/QFont>
#include <QtWidgets/QGraphicsItem>

#include "Frame.h"

QT_BEGIN_NAMESPACE
class QGraphicsSceneMouseEvent;
QT_END_NAMESPACE

QT_BEGIN_NAMESPACE
class QChart;
QT_END_NAMESPACE

QT_USE_NAMESPACE

const int kDefaultBarWidth = 2.0;
static int kBoundingWidth = 2000;
static int kBoundingHeight = 1000;
static int kMaxWidth = 100;
static const int kPenWidth = 1;
static const int kBarWidth = 2;
const int kClipLineWidth = 3;
const float kClipTextOffsetX = -4.0;
const float kClipTextOffsetY = 4.0;
const int kClipAnchorSize = 10;
const int kClipAnchorOffsetY = kClipAnchorSize;

class FrameGraphicsItem : public QObject, public QGraphicsItem {
  Q_OBJECT
 public:
  FrameGraphicsItem(QChart* parent);
  ~FrameGraphicsItem();

  QRectF boundingRect() const override;

  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
             QWidget* widget) override;

  void Initialize(const Frame* frame, QPointF point, float width);
  void UpdateGeometry();
  void ShowGpuTime(bool show) { show_gpu_time_ = show; }
  void ShowDriverTime(bool show) { show_driver_time_ = show; }
  void ShowPresentApiTime(bool show) { show_present_api_time_ = show; }
  void ShowDisplayQueueTime(bool show) { show_display_queue_time_ = show; }
  void ShowAppTime(bool show) { show_app_time_ = show; }
  void UpdateWidth(qreal factor) {
    width_ *= factor;
    if (width_ >= kMaxWidth) {
      width_ = kMaxWidth;
    } else if (width_ < kDefaultBarWidth) {
      width_ = kDefaultBarWidth;
    }
    this->UpdateGeometry();
  }
  const Frame* GetFrame() { return frame_data_; }

 public slots:
  void zoomReset() {
    width_ = reset_width_;
    this->UpdateGeometry();
  };

 signals:
  void hoverEntered(QPointF scene_point, float fps);
  void hoverLeft();

 protected:
  void hoverEnterEvent(QGraphicsSceneHoverEvent* event);
  void hoverLeaveEvent(QGraphicsSceneHoverEvent* event);

 private:
  const Frame* frame_data_;

  QRectF rect_;
  float width_;
  float reset_width_;
  QPointF anchor_;
  QChart* chart_;

  bool show_gpu_time_;
  bool show_driver_time_;
  bool show_present_api_time_;
  bool show_display_queue_time_;
  bool show_app_time_;
  QGraphicsLineItem* text_line_;
  QGraphicsTextItem* frame_text_;
};

class ClippingLineGraphicsItem : public QObject, public QGraphicsLineItem {
  Q_OBJECT
 public:
  ClippingLineGraphicsItem(const QLineF& line, qreal left, qreal right,
                           QChart* parent);
  ~ClippingLineGraphicsItem();
  QRectF boundingRect() const override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
             QWidget* widget) override;

  qreal GetTime() { return time_; }
  void UpdateGeometry();
  void SetLeftBound(qreal left);
  void SetRightBound(qreal right);
  bool IsHovered() { return hovered_; }

 public slots:
  void SetTime(float time) {
    time_ = time;
    UpdateGeometry();
  }

 signals:
  void TimeChanged(float time);

 protected:
  void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
  void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
  QVariant itemChange(GraphicsItemChange change,
                      const QVariant& value) override;

 private:
  void UpdateAnchors();
  QChart* chart_;
  qreal time_;  // time value corresponding to x-axis
  qreal left_;
  qreal right_;
  bool hovered_;
  QGraphicsTextItem* line_text_;
  QGraphicsRectItem* bottom_anchor_;
  QGraphicsRectItem* top_anchor_;
};