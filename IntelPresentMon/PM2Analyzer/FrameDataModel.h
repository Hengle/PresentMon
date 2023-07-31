#pragma once

#include <QtCore/QAbstractTableModel>

#include "DataModel.h"

enum class FrameDataTableColumns {
    kFileName = 0,
    kProcessName = 1,
    kAvgFps,
    k99thPercentileFrameTime,
    k95thPercentileFrameTime,
    k90thPercentileFrameTime,
    kTotalFrameData
};

// This class holds all of the current loaded data and acts as a tablemodel for
// the file list view
class FrameDataModel : public QAbstractTableModel {
    Q_OBJECT
   public:
    FrameDataModel(QObject* parent = 0);
    std::shared_ptr<DataModel> dataModel(int index);
    bool ValidFile(const std::string& fileName);

    int GetDataRowIndex(const std::string& file_name);
    int AddData(const std::string& fileName, bool dir);
    void RemoveData(int first, int last);

    bool SetModels(const std::vector<std::shared_ptr<DataModel>>& data_models_);

    int rowCount(const QModelIndex& parent = QModelIndex()) const;
    int columnCount(const QModelIndex& parent = QModelIndex()) const;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const;
    bool setData(const QModelIndex& index, const QVariant& value,
                 int role = Qt::EditRole);
    Qt::ItemFlags flags(const QModelIndex& index) const;

    // Keep all the data in here
    std::vector<std::shared_ptr<DataModel>> data_models_;

   public slots:
    void Cancel(){};
};
