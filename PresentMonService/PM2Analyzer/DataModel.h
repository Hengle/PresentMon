#pragma once
#include <QtCore/QAbstractTableModel>
#include <QtCore/QHash>
#include <QtCore/QRect>
#include <vector>

#include "Frame.h"
#include "Series.h"

// Model for one dataset
class DataModel : public QAbstractTableModel {
  Q_OBJECT

 public:
  // Struct for statistics
  struct Stats {
    Stats()
        : avg(0),
          median(0),
          _90thpercentile(0),
          _95thpercentile(0),
          _99thpercentile(0),
          duration(0) {}
    float avg;
    float median;
    float _90thpercentile;
    float _95thpercentile;
    float _99thpercentile;

    float duration;
  };

  explicit DataModel(QObject* parent = 0);
  bool loadData(const std::string fileName);

  int rowCount(const QModelIndex& parent = QModelIndex()) const;
  int columnCount(const QModelIndex& parent = QModelIndex()) const;
  QVariant headerData(int section, Qt::Orientation orientation,
                      int role = Qt::DisplayRole) const;
  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const;
  bool setData(const QModelIndex& index, const QVariant& value,
               int role = Qt::EditRole);
  Qt::ItemFlags flags(const QModelIndex& index) const;

  std::vector<std::shared_ptr<Frame>>& frames();

  QString fname_;
  QString game_name_;
  QString process_name_;

  // Interface for statistics
  void calcStats();
  void SetStartTime(float start_time) { start_time_in_ms_ = start_time; }
  void SetEndTime(float end_time) { end_time_in_ms_ = end_time; }
  void SetStatsWindw(float start_time, float end_time) {
    start_time_in_ms_ = start_time;
    end_time_in_ms_ = end_time;
  }

  Stats stats;

  // Offset of sample
  float first_frame_time_in_ms = 0;
  float start_time_in_ms_ = 0;
  float end_time_in_ms_ = 0;
  int first_frame_id_ = 0;

 private:
  // Raw data
  std::vector<std::shared_ptr<Frame>> frameData;

  // Statistic Data
  std::vector<float> ms_btw_presents_vect_;
};
