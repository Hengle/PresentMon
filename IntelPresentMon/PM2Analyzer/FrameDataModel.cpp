#include "FrameDataModel.h"

#include <QFileInfo>
#include <QMessageBox>
#include <QtCore/QRandomGenerator>
#include <QtCore/QRect>
#include <QtCore/QVector>
#include <QtGui/QColor>
#include <iostream>

#include "MSG.h"

FrameDataModel::FrameDataModel(QObject* parent) : QAbstractTableModel(parent) {}

// Access for datamodel
std::shared_ptr<DataModel> FrameDataModel::dataModel(int index) {
  if (data_models_.size() > index)
    return data_models_[index];
  else
    return nullptr;
}

// Check for a valid filetype
bool FrameDataModel::ValidFile(const std::string& fileName) { return true; }

int FrameDataModel::GetDataRowIndex(const std::string& file_name) {
  for (int i = 0; i < data_models_.size(); i++) {
    if (file_name.compare(data_models_[i]->fname_.toStdString()) == 0) {
      return i;
    }
  }

  return -1;
}

// Append data to the model
int FrameDataModel::AddData(const std::string& fileName, bool dir) {
  std::string full_path = fileName;
  QFileInfo fi(full_path.c_str());
  if (!fi.exists()) {
    InfoMessage("Test data " + fileName + " doesn't exist.");
    return -1;
  }
  // Create a new datamodel and add to our store
  if (!dir) {
    // Check if file is already loaded
    for (int i = 0; i < data_models_.size(); i++) {
      std::string base_name = fi.baseName().toStdString();
      if (base_name.compare(data_models_[i]->fname_.toStdString()) == 0) {
        QMessageBox msgBox;
        msgBox.setText(
            QString::fromStdString(fileName + " has already been loaded."));
        msgBox.exec();
        return -1;
      }
    }

    std::shared_ptr<DataModel> dataModel = std::make_shared<DataModel>();

    if (dataModel->loadData(full_path)) {
      int row = data_models_.size();

      // Add to the vector of datamodels
      beginInsertRows(QModelIndex(), row, row);
      data_models_.push_back(dataModel);
      endInsertRows();

      return row;
    }
  }

  return -1;
}

void FrameDataModel::RemoveData(int first, int last) {
  auto parent = QModelIndex();
  beginRemoveRows(QModelIndex(), first, last);
  data_models_.erase(data_models_.begin() + first,
                     data_models_.begin() + last + 1);
  endRemoveRows();
}

bool FrameDataModel::SetModels(
    const std::vector<std::shared_ptr<DataModel>>& data_models_) {
  this->data_models_ = data_models_;
  return true;
}

int FrameDataModel::rowCount(const QModelIndex& parent) const {
  Q_UNUSED(parent)
  return data_models_.size();
}

int FrameDataModel::columnCount(const QModelIndex& parent) const {
  Q_UNUSED(parent)
  return static_cast<int>(FrameDataTableColumns::kTotalFrameData);
}

QVariant FrameDataModel::headerData(int section, Qt::Orientation orientation,
                                    int role) const {
  if (role != Qt::DisplayRole) return QVariant();

  if (orientation == Qt::Horizontal) {
    switch (section) {
      case 0:
        return "DataFile";
      case 1:
        return "Process Name";
      case 2:
        return "Avg FPS";
      case 3:
        return "99th percentile";
      case 4:
        return "95th percentile";
      case 5:
        return "90th percentile";
      default:
        return QString("%1").arg(section + 1);
    }
  }

  return QVariant();
}

QVariant FrameDataModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || role != Qt::DisplayRole) {
    return QVariant();
  }

  switch (index.column()) {
    case 0:
      return data_models_[index.row()]->fname_;
    case 1:
      return data_models_[index.row()]->process_name_;
    case 2:
      return data_models_[index.row()]->stats.avg;
    case 3:
      return data_models_[index.row()]->stats._99thpercentile;
    case 4:
      return data_models_[index.row()]->stats._95thpercentile;
    case 5:
      return data_models_[index.row()]->stats._90thpercentile;

    default:
      return QVariant();
  }
}

bool FrameDataModel::setData(const QModelIndex& index, const QVariant& value,
                             int role) {
  if (index.isValid() && role == Qt::EditRole) {
    if (data_models_.size() > index.row()) {
      data_models_[index.row()]->fname_ = value.toString();
      emit dataChanged(index, index);
      return true;
    }
  }
  return false;
}

Qt::ItemFlags FrameDataModel::flags(const QModelIndex& index) const {
  return QAbstractItemModel::flags(index) | Qt::ItemIsEditable;
}
