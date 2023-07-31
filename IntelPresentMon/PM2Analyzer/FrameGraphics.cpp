#include "FrameGraphics.h"

#include <QDebug>
#include <QtGui/QFontMetrics>
#include <QtGui/QPainter>
#include <QtWidgets/QGraphicsSceneMouseEvent>
#include <sstream>

#include "ColorPalette.h"
#include "Frame.h"
#include "Logger.h"

void FrameGraphicsItem::hoverEnterEvent(QGraphicsSceneHoverEvent* event) {
  QPointF text_pos =
      mapToScene(mapFromParent(chart_->mapToPosition(this->anchor_)));

  if (!frame_text_) {
    frame_text_ = new QGraphicsTextItem(this);
    QString frame_details(
        "<table border = '0' style=\"background-color:#FFFFE0\"><tr><th "
        "style=\"text-align:left\">Frame# " +
        QString::number(frame_data_->frame_num_) + "</th><td></td></tr>" +

        "<tr><th style=\"text-align: left\">Time since beginning(ms): "
        "<\th><td>" +
        QString::number(this->anchor_.x()) +
        "</td></tr>"
        "<tr><th style=\"text-align: left\">Time btw Presents(ms): </th><td>" +
        QString::number(frame_data_->ms_btw_presents_) + "<\td>" +
        "<tr><th style=\"text-align: left\">Time in Presents(ms): </th><td>" +
        QString::number(frame_data_->ms_in_present_api_) + "</td></tr>" +
        "<tr><th style=\"text-align: left\">GPU time(ms): </th><td>" +
        QString::number(frame_data_->gpu_time_ms_) + "</td></tr>" +
        "<tr><th style=\"text-align: left\">Render start(ms): </th><td>" +
        QString::number(frame_data_->normalized_gpu_start_) + "</td></tr>" +
        "<tr><th style=\"text-align: left\">Render complete(ms): </th><td>" +
        QString::number(frame_data_->normalized_gpu_end_) + "</td></tr>" +
        "<tr><th style=\"text-align: left\">App start(ms): </th><td>" +
        QString::number(frame_data_->normalized_app_start_) + "</td></tr>" +
        "<tr><th style=\"text-align: left\"> Animation Error(ms): "
        "</th><td>" +
        QString::number(frame_data_->animation_error_) + "</td></tr>");

    frame_text_->setZValue(11);
    frame_text_->setParentItem(nullptr);
    frame_text_->setHtml(
        QString("<div style='background:rgba(255, 255, 255, 100%);'>" +
                frame_details + QString("</div>")));
  }

  if (!text_line_) {
    text_line_ = new QGraphicsLineItem(this);
    text_line_->setLine(0, 0, 0, chart_->plotArea().height());
    text_line_->setParentItem(nullptr);
    text_line_->setZValue(11);
  }

  text_line_->setPos(text_pos.x(), 0);
  text_line_->setVisible(true);
  frame_text_->setPos(text_pos.x(), chart_->plotArea().height() * 0.9);
  frame_text_->setVisible(true);

  hoverEntered(text_pos, frame_data_->fps_presents_);
}

void FrameGraphicsItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* event) {
  if (text_line_) {
    text_line_->setVisible(false);
  }
  if (frame_text_) {
    frame_text_->setVisible(false);
  }

  hoverLeft();
}

FrameGraphicsItem::FrameGraphicsItem(QChart* chart)
    : QGraphicsItem(chart),
      chart_(chart),
      show_gpu_time_(true),
      show_driver_time_(true),
      show_present_api_time_(true),
      show_app_time_(true),
      show_display_queue_time_(true),
      frame_data_(nullptr),
      width_(kDefaultBarWidth),
      reset_width_(kDefaultBarWidth),
      text_line_(Q_NULLPTR),
      frame_text_(Q_NULLPTR) {
  anchor_ = QPointF(0.0, 0.0);
  rect_ = QRectF(0.0, 0.0, 0.0, 0.0);
  setAcceptHoverEvents(true);
  setFlag(QGraphicsItem::ItemClipsToShape, true);
}

FrameGraphicsItem::~FrameGraphicsItem() {
  if (text_line_) {
    delete text_line_;
  }
  if (frame_text_) {
    delete frame_text_;
  }
}

QRectF FrameGraphicsItem::boundingRect() const {
  QPointF anchor = this->anchor_;
  QPointF anchor_to_pos = chart_->mapToPosition(QPointF(this->anchor_.x(), 0));
  QPointF anchor_from_parent = mapFromParent(anchor_to_pos);

  QRectF rect;
  rect.setLeft(anchor_from_parent.x() - width_ / 2);
  rect.setRight(anchor_from_parent.x() + width_ / 2);
  rect.setTop(anchor_from_parent.y() - 1000);
  rect.setBottom(anchor_from_parent.y() + 1000);

  return rect;
}

void FrameGraphicsItem::paint(QPainter* painter,
                              const QStyleOptionGraphicsItem* option,
                              QWidget* widget) {
  painter->setRenderHint(QPainter::Antialiasing);
  Q_UNUSED(option)
  Q_UNUSED(widget)

  QPen pen;  // creates a default pen
  pen.setWidth(1);
  pen.setBrush(QColor::fromRgb(0x62, 0xbe, 0xd2));
  painter->setPen(pen);

  float frame_num_ = this->frame_data_->frame_num_;
  auto pos = chart_->mapToPosition(this->anchor_);
  QPointF anchor = mapFromParent(pos);

  setZValue(0);

  if (frame_data_->dropped_) return;

  // App Start Time
  auto app_start_pos = chart_->mapToPosition(
      QPointF(anchor_.x(), frame_data_->normalized_app_start_));

  if (!(chart_->plotArea().contains(app_start_pos.x(),
                                    (app_start_pos.y() + pos.y()) / 2)))
    return;

  QPointF app_start = mapFromParent(app_start_pos);

  // GPU Time
  pos = chart_->mapToPosition(
      QPointF(anchor_.x(), frame_data_->normalized_gpu_start_));
  QPointF gpu_start = mapFromParent(pos);
  pos = chart_->mapToPosition(
      QPointF(anchor_.x(), frame_data_->normalized_gpu_end_));
  QPointF gpu_end = mapFromParent(pos);

  // PresentTime
  pos = chart_->mapToPosition(
      QPointF(anchor_.x(), frame_data_->normalized_present_start_));
  QPointF present_start = mapFromParent(pos);
  pos = chart_->mapToPosition(QPointF(
      anchor_.x(), static_cast<double>(frame_data_->normalized_present_start_) +
                       static_cast<double>(frame_data_->ms_in_present_api_)));
  QPointF present_end = mapFromParent(pos);

  // Driver Time
  pos = chart_->mapToPosition(
      QPointF(anchor_.x(), frame_data_->normalized_driver_start_));
  QPointF driver_start = mapFromParent(pos);
  pos = chart_->mapToPosition(QPointF(
      anchor_.x(), static_cast<double>(frame_data_->normalized_driver_start_) +
                       static_cast<double>(frame_data_->driver_time_ms_)));
  QPointF driver_end = mapFromParent(pos);

  // Display Queue Time
  pos = chart_->mapToPosition(
      QPointF(anchor_.x(), frame_data_->normalized_scheduled_flip_));
  QPointF queue_start = mapFromParent(pos);
  pos = chart_->mapToPosition(
      QPointF(anchor_.x(), frame_data_->normalized_actual_flip_));
  QPointF queue_end = mapFromParent(pos);

  // Flip Time
  pos = chart_->mapToPosition(
      QPointF(anchor_.x(), frame_data_->normalized_actual_flip_));
  QPointF flip = mapFromParent(pos);

  painter->fillRect(anchor.x() - width_ / 2, app_start.y(), width_,
                    anchor.y() - app_start.y(), kFrameTimeColor);

  if (show_display_queue_time_) {
    painter->fillRect(anchor.x() - width_ / 2, queue_start.y(), width_,
                      queue_end.y() - queue_start.y(), kDisplayQueueTimeColor);
  }

  if (show_gpu_time_) {
    painter->fillRect(anchor.x() - width_ / 2, gpu_start.y(), width_,
                      gpu_end.y() - gpu_start.y(), kGpuTimeColor);
  }

  if (show_driver_time_) {
    painter->fillRect(anchor.x() - width_ / 2, driver_start.y(), width_,
                      driver_end.y() - driver_start.y(), kDriverTimeColor);
  }

  if (show_present_api_time_) {
    painter->fillRect(anchor.x() - width_ / 2, present_start.y(), width_,
                      present_end.y() - present_start.y(),
                      kPresentApiTimeColor);
  }

  if (show_app_time_) {
    painter->fillRect(anchor.x() - width_ / 2, app_start.y(), width_,
                      present_start.y() - app_start.y(), kAppTimeColor);
  }
}

void FrameGraphicsItem::Initialize(const Frame* frame, QPointF anchor_point,
                                   float width) {
  frame_data_ = frame;
  anchor_ = anchor_point;
  width_ = width;
  reset_width_ = width;
  rect_ = boundingRect();
}

void FrameGraphicsItem::UpdateGeometry() {
  prepareGeometryChange();
  auto pos = chart_->mapToPosition(anchor_);
  setPos(pos);
}

ClippingLineGraphicsItem::ClippingLineGraphicsItem(const QLineF& line,
                                                   qreal left, qreal right,
                                                   QChart* chart)
    : QGraphicsLineItem(line, chart),
      chart_(chart),
      left_(left),
      right_(right),
      hovered_(false),
      line_text_(nullptr),
      bottom_anchor_(nullptr),
      top_anchor_(nullptr) {
  setFlag(QGraphicsItem::ItemIsMovable, true);
  setFlag(QGraphicsItem::ItemIsSelectable, true);
  setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);

  auto const line_pos = chart_->mapFromScene(line.p1());
  auto const time_pos = chart_->mapToValue(line_pos);

  time_ = time_pos.x();

  bottom_anchor_ =
      new QGraphicsRectItem(0, 0, kClipAnchorSize, kClipAnchorSize, this);
  bottom_anchor_->setVisible(true);
  bottom_anchor_->setAcceptHoverEvents(true);
  bottom_anchor_->setAcceptedMouseButtons(Qt::LeftButton);

  top_anchor_ =
      new QGraphicsRectItem(0, 0, kClipAnchorSize, kClipAnchorSize, this);
  top_anchor_->setVisible(true);
  top_anchor_->setAcceptHoverEvents(true);
  top_anchor_->setAcceptedMouseButtons(Qt::LeftButton);

  setZValue(20);
  setAcceptHoverEvents(true);
}

ClippingLineGraphicsItem::~ClippingLineGraphicsItem() {
  if (line_text_) {
    delete line_text_;
  }
}

void ClippingLineGraphicsItem::SetLeftBound(qreal left) { left_ = left; }

void ClippingLineGraphicsItem::SetRightBound(qreal right) { right_ = right; }

void ClippingLineGraphicsItem::mousePressEvent(
    QGraphicsSceneMouseEvent* event) {
  QGraphicsItem::mousePressEvent(event);
}

void ClippingLineGraphicsItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event) {
  if (isSelected()) {
    auto scenePos = event->scenePos();

    if (scenePos.x() < chart_->plotArea().x()) {
      scenePos.setX(chart_->plotArea().x());
    } else if (scenePos.x() >
               chart_->plotArea().x() + chart_->plotArea().width()) {
      scenePos.setX(chart_->plotArea().x() + chart_->plotArea().width());
    }
    auto const chartItemPos = chart_->mapFromScene(scenePos);
    auto const valueGivenSeries = chart_->mapToValue(chartItemPos);

    time_ = valueGivenSeries.x();
    if (time_ < left_) {
      time_ = left_;
    } else if (time_ > right_) {
      time_ = right_;
    }

    UpdateGeometry();
  }
  QGraphicsItem::mouseMoveEvent(event);
}

void ClippingLineGraphicsItem::mouseReleaseEvent(
    QGraphicsSceneMouseEvent* event) {
  QGraphicsItem::mouseReleaseEvent(event);
}

QVariant ClippingLineGraphicsItem::itemChange(GraphicsItemChange change,
                                              const QVariant& value) {
  if (change == ItemPositionChange && isSelected()) {
    // Overwriting position change. Paint will handle the line position
    TimeChanged(time_);
    return (0, 0);
  }
  return QGraphicsItem::itemChange(change, value);
}

QRectF ClippingLineGraphicsItem::boundingRect() const {
  auto map_to_pos = chart_->mapToPosition(QPointF(time_, 0));
  auto map_from_parent = this->mapFromParent(map_to_pos);
  auto map_to_scene = this->mapToScene(map_from_parent);

  QRectF rect;
  rect.setLeft(map_to_scene.x() - 100);
  rect.setRight(map_to_scene.x() + 100);
  rect.setTop(this->line().p1().y() - kClipAnchorOffsetY - kClipTextOffsetY);
  rect.setBottom(this->line().p2().y() + kClipAnchorOffsetY);

  return rect;
}

void ClippingLineGraphicsItem::UpdateAnchors() {
  QPointF map_to_pos = chart_->mapToPosition(QPointF(time_, 0));
  QPointF map_from_parent = this->mapFromParent(map_to_pos);
  QPointF map_to_scene = this->mapToScene(map_from_parent);
  bottom_anchor_->setPos(map_to_scene.x() - kClipAnchorSize / 2,
                         this->line().p2().y() - kClipAnchorOffsetY);

  top_anchor_->setPos(map_to_scene.x() - kClipAnchorSize / 2,
                      this->line().p1().y());
}

void ClippingLineGraphicsItem::paint(QPainter* painter,
                                     const QStyleOptionGraphicsItem* option,
                                     QWidget* widget) {
  // QGraphicsLineItem::paint(painter, option, widget);
  Q_UNUSED(option);
  Q_UNUSED(widget);

  QPointF map_to_pos = chart_->mapToPosition(QPointF(time_, 0));
  QPointF map_from_parent = this->mapFromParent(map_to_pos);
  QPointF map_to_scene = this->mapToScene(map_from_parent);

  if (!chart_->plotArea().contains(map_to_pos)) return;

  QPen pen(kClipLineColor);
  pen.setWidth(kClipLineWidth);
  if (isSelected()) {
    bottom_anchor_->setBrush(kClipLinePressColor);
    top_anchor_->setBrush(kClipLinePressColor);
    pen.setColor(kClipLinePressColor);
  } else if (hovered_) {
    bottom_anchor_->setBrush(kClipLineHoverColor);
    top_anchor_->setBrush(kClipLineHoverColor);
    pen.setColor(kClipLineHoverColor);
  } else {
    bottom_anchor_->setBrush(QBrush(kClipLineColor));
    top_anchor_->setBrush(QBrush(kClipLineColor));
    pen.setColor(kClipLineColor);
  }

  painter->setPen(pen);

  UpdateAnchors();

  painter->drawLine(this->line());
  painter->drawText(QPointF(map_to_scene.x() + kClipTextOffsetX,
                            this->line().p1().y() - kClipTextOffsetY),
                    QString::number(time_));
}

void ClippingLineGraphicsItem::UpdateGeometry() {
  prepareGeometryChange();
  auto map_to_pos = chart_->mapToPosition(QPointF(time_, 0));
  auto map_from_parent = this->mapFromParent(map_to_pos);
  auto map_to_scene = this->mapToScene(map_from_parent);

  this->setLine(QLineF(map_to_scene.x(), this->line().p1().y(),
                       map_to_scene.x(), this->line().p2().y()));
  UpdateAnchors();
}
void ClippingLineGraphicsItem::hoverEnterEvent(
    QGraphicsSceneHoverEvent* event) {
  hovered_ = true;
  QGraphicsItem::hoverEnterEvent(event);
}

void ClippingLineGraphicsItem::hoverLeaveEvent(
    QGraphicsSceneHoverEvent* event) {
  hovered_ = false;
  QGraphicsItem::hoverEnterEvent(event);
}