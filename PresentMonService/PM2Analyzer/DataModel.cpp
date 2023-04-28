#include "DataModel.h"

#include <QFileInfo>
#include <QSettings>
#include <QtCore/QRandomGenerator>
#include <QtCore/QRect>
#include <QtCore/QVector>
#include <QtGui/QColor>
#include <iostream>

#include "CSVParser.h"
#include "InputFileTypes.h"
#include "MSG.h"

DataModel::DataModel(QObject* parent) : QAbstractTableModel(parent) {
  QSettings settings("Moose Soft", "Clipper");

  settings.beginGroup("Loader");

  QString sText = settings.value("text", "").toString();
  settings.endGroup();
}

// Read a csv and load frame data
bool DataModel::loadData(const std::string fileName) {
  CSVParser parser;
  InputFileTypesProcessor processor;

  QFileInfo fi(fileName.c_str());
  if (!fi.exists()) {
    InfoMessage("Test data " + fileName + " doesn't exist.");
    return false;
  }
  fname_ = fi.baseName();

  // Parse
  std::vector<Row> data;
  if (parser.Parse(fileName, data)) {
    // Find the input file the signature
    InputFileTypesProcessor::inputSig sig =
        processor.Parse(fileName, data, frameData);

    if (sig == InputFileTypesProcessor::inputSig::Error) {
      return false;
    }

    // Update the offset to the first starttime
    first_frame_time_in_ms = frameData[0]->time_in_ms_;
    start_time_in_ms_ = frameData[0]->time_in_ms_ - first_frame_time_in_ms;
    end_time_in_ms_ = frameData.back()->time_in_ms_ - first_frame_time_in_ms;
    first_frame_id_ = frameData[0]->frame_num_;
    process_name_ = QString::fromUtf8(frameData[0]->application_.c_str());

    calcStats();
    return true;
  }

  return false;
}

int DataModel::rowCount(const QModelIndex& parent) const {
  Q_UNUSED(parent)
  return frameData.size();
}

int DataModel::columnCount(const QModelIndex& parent) const {
  Q_UNUSED(parent)
  return 2;
}

QVariant DataModel::headerData(int section, Qt::Orientation orientation,
                               int role) const {
  if (role != Qt::DisplayRole) return QVariant();

  if (orientation == Qt::Horizontal) {
    if (section % 2 == 0)
      return "Frame#";
    else
      return "FrameTime";
  } else {
    return QString("%1").arg(section + 1);
  }
}

QVariant DataModel::data(const QModelIndex& index, int role) const {
  if ((role == Qt::DisplayRole) || (role == Qt::EditRole)) {
    switch (index.column()) {
      case 0:
        return frameData[index.row()]->frame_num_;
      case 1:
        return frameData[index.row()]->ms_until_displayed_;

      default:
        return QVariant();
    }
  }

  else if (role == Qt::BackgroundRole) {
    // cell not mapped return white color
    return QColor(Qt::white);
  }
  return QVariant();
}

bool DataModel::setData(const QModelIndex& index, const QVariant& value,
                        int role) {
  if (index.isValid() && role == Qt::EditRole) {
    if (index.column() == 0) {
      frameData[index.row()]->frame_num_ = value.toInt();
    } else if (index.column() == 1) {
      frameData[index.row()]->ms_btw_presents_ = value.toDouble();
    }

    emit dataChanged(index, index);
    return true;
  }
  return false;
}

Qt::ItemFlags DataModel::flags(const QModelIndex& index) const {
  return QAbstractItemModel::flags(index);
}

void DataModel::calcStats() {
  float total_time = 0.0;
  int frames = frameData.size();
  float first_frame_time_in_ms = frameData[0]->time_in_ms_;

  ms_btw_presents_vect_.clear();

  for (auto f : frameData) {
    if (((f->time_in_ms_ - f->first_frame_time_in_ms_) >= start_time_in_ms_) &&
        ((f->time_in_ms_ - f->first_frame_time_in_ms_) <= end_time_in_ms_)) {
      ms_btw_presents_vect_.push_back(f->ms_btw_presents_);
    }
  }

  if (ms_btw_presents_vect_.size() == 0) {
    return;
  }

  // Find start and end index for the stats window
  int start_idx = 0;
  int end_idx = ms_btw_presents_vect_.size() - 1;

  auto start_iter = ms_btw_presents_vect_.begin();
  auto end_iter = ms_btw_presents_vect_.end();

  int window_size = ms_btw_presents_vect_.size();
  int median_idx = window_size * (50.0 / 100);
  int percentile_90_idx = window_size * (90.0 / 100);
  int percentile_95_idx = window_size * (95.0 / 100);
  int percentile_99_idx = window_size * (99.0 / 100);

  std::sort(start_iter, end_iter, std::greater<float>());
  float sum = std::accumulate(start_iter, end_iter, 0.0);
  if (sum != 0) {
    stats.avg = window_size * 1000 / sum;
  }

  auto median_iter = start_iter;
  std::advance(median_iter, median_idx);
  auto percentile_90_iter = start_iter;
  std::advance(percentile_90_iter, percentile_90_idx);
  auto percentile_95_iter = start_iter;
  std::advance(percentile_95_iter, percentile_95_idx);
  auto percentile_99_iter = start_iter;
  std::advance(percentile_99_iter, percentile_99_idx);

  stats._99thpercentile = 1000.0 / (*percentile_99_iter);
  stats._95thpercentile = 1000.0 / (*percentile_95_iter);
  stats._90thpercentile = 1000.0 / (*percentile_90_iter);
  stats.median = 1000.0 / (*median_iter);
}

std::vector<std::shared_ptr<Frame>>& DataModel::frames() { return frameData; }
