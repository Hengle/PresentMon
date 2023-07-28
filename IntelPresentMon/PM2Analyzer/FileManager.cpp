#include "FileManager.h"

#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QDrag>
#include <QItemSelectionModel>
#include <QMdiSubWindow>
#include <QMimeData>
#include <QObject>
#include <QScreen>
#include <QSettings>
#include <QString>
#include <QWidget>
#include <QtGui>
#include <QtPrintSupport/QPrintDialog>
#include <QtPrintSupport/QPrinter>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QMessageBox>
#include <iostream>
#include <memory>
#include <vector>

#include "ColorPalette.h"
#include "FrameDataModel.h"
#include "Logger.h"
#include "MSG.h"
#include "PerfCheck.h"

enum class StatsData : int { kAvgFps = 0, k99Fps, k95Fps, k90Fps, kTotal };

// Slot for Print button
void FileManager::PrintChart() {
  for (auto pc : ws) {
    pc->Print();
  }
}

void FileManager::PrintStatsTable() {
  if (file_table_view_ != nullptr) {
    std::unique_ptr<QPrinter> printer = std::make_unique<QPrinter>();
    printer->setPageOrientation(QPageLayout::Landscape);
    printer->setResolution(QPrinter::HighResolution);

    std::unique_ptr<QPrintDialog> dialog =
        std::make_unique<QPrintDialog>(printer.get());
    dialog->setWindowTitle("Exporting Stats Summary");
    if (dialog->exec() != QDialog::Accepted) return;
    std::unique_ptr<QPainter> painter = std::make_unique<QPainter>();
    painter->begin(printer.get());
    file_table_view_->render(painter.get());

    painter->end();
  }
}

void FileManager::PrintStatsSummary() {
  if (stats_chart_view_ != nullptr) {
    std::unique_ptr<QPrinter> printer = std::make_unique<QPrinter>();
    printer->setPageOrientation(QPageLayout::Landscape);
    printer->setResolution(QPrinter::HighResolution);

    std::unique_ptr<QPrintDialog> dialog =
        std::make_unique<QPrintDialog>(printer.get());
    dialog->setWindowTitle("Exporting Stats Summary");
    if (dialog->exec() != QDialog::Accepted) return;
    std::unique_ptr<QPainter> painter = std::make_unique<QPainter>();
    painter->begin(printer.get());
    stats_chart_view_->render(painter.get());

    painter->end();
  }
}

FileManager::FileManager(QWidget* parent)
    : QMainWindow(parent),
      stats_chart_(nullptr),
      stats_chart_view_(nullptr),
      series_(nullptr),
      axis_x_(nullptr) {
  LogMessage("FileManager Created");

  setAcceptDrops(true);

  // Restore last settings
  ReadSettings();

  // Create the global strorage for all frame data
  file_list_model_ = std::make_unique<FrameDataModel>();

  ui_.setupUi(this);
  mdi_area_ =
      ui_.centralwidget->findChild<QMdiArea*>("mdiArea");  // init QMdiArea
  file_table_view_ = ui_.centralwidget->findChild<QTableView*>("FileTable");
  file_table_view_->setModel(file_list_model_.get());
  file_table_view_->setSelectionBehavior(QAbstractItemView::SelectRows);
  file_table_view_->horizontalHeader()->setSectionResizeMode(
      QHeaderView::Stretch);

  InitializeStatsChart();

  // Menus
  load_action_ = ui_.actionLoad;
  connect(load_action_, SIGNAL(triggered()), this, SLOT(LoadFile()));
  export_chart_action_ = ui_.actionChart;
  connect(export_chart_action_, SIGNAL(triggered()), this, SLOT(PrintChart()));
  export_stats_graph_action_ = ui_.actionStatsGraph;
  connect(export_stats_graph_action_, SIGNAL(triggered()), this,
          SLOT(PrintStatsSummary()));
  export_stats_table_action_ = ui_.actionStatsTable;
  connect(export_stats_table_action_, SIGNAL(triggered()), this,
          SLOT(PrintStatsTable()));

  QTextEdit* msgPtr = ui_.centralwidget->findChild<QTextEdit*>("msgPanel");
  myMSG::setMessagePanel(msgPtr);
}

void FileManager::launch() { launchSelected(); }

void FileManager::launchSelected() {
  QItemSelectionModel* select = file_table_view_->selectionModel();
  connect(
      select,
      SIGNAL(selectionChanged(const QItemSelection&, const QItemSelection&)),
      this,
      SLOT(OnSelectionChange(const QItemSelection&, const QItemSelection&)));
  QModelIndexList selection = select->selectedRows();  // return selected row(s)

  std::vector<std::shared_ptr<DataModel>> selectedDataModels;

  // Multiple rows can be selected
  for (int i = 0; i < selection.count(); i++) {
    QModelIndex index = selection.at(i);
    std::shared_ptr<DataModel> d = file_list_model_->dataModel(index.row());
    selectedDataModels.push_back(d);
  }

  // Launch a child if data not exist
  PerfCheck* perf_check = new PerfCheck(this, this->mdi_area_);
  perf_check->SetDataModels(selectedDataModels);
  perf_check->setWindowTitle(selectedDataModels[0]->fname_ + ":" +
                             selectedDataModels[0]->process_name_);
  bool result = perf_check->BuildBarSeries(selectedDataModels[0]);

  if (!result) {
    delete perf_check;
    return;
  }

  connect(perf_check, SIGNAL(StatsChanged(QModelIndex, QModelIndex)), this,
          SLOT(dataChanged(QModelIndex, QModelIndex)));
  connect(perf_check, SIGNAL(UpdateStatsSummary()), this,
          SLOT(UpdateStatsChart()));
  ws.push_back(perf_check);

  // Launch sub window
  UpdateStatsChart();

  QMdiSubWindow* sub_window = new QMdiSubWindow;
  sub_window->setWidget(perf_check);
  sub_window->setAttribute(Qt::WA_DeleteOnClose);
  mdi_area_->addSubWindow(sub_window);

  sub_window->show();
  perf_check->setFocus();
}

void FileManager::kill() {
  // Kill oldest
  if (ws.size()) ws.erase(ws.begin());
}

// Save and restore settings
void FileManager::WriteSettings() {
  QSettings settings("TAPApp", "FileManager");

  settings.beginGroup("Loaded Items");
  settings.endGroup();
}

void FileManager::ReadSettings() {
  QSettings settings("TAPApp", "FileManager");

  settings.beginGroup("Loaded Items");
  settings.endGroup();
}

// Given a URLlist add to the filelist model
void FileManager::addUrls(const QList<QUrl>& urlList) {
  int cnt = 0;

  // extract the local paths of the files
  for (int i = 0; i < urlList.count(); ++i) {
    const QString& s = urlList[i].toLocalFile();
    QFileInfo check_file(s);
    QString n = check_file.filePath();
    bool dir = check_file.isDir();

    if (check_file.exists()) {
      int row = file_list_model_->AddData(n.toStdString(), dir);
      if (row >= 0) {
        file_table_view_->clearSelection();
        file_table_view_->selectRow(row);
        file_table_view_->setFocus();
        file_table_view_->resizeColumnsToContents();
        launchSelected();
      }

      if (dir) {
        QDirIterator it(s, QStringList() << "*.csv",
                        QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
                        QDirIterator::Subdirectories);

        while (it.hasNext()) {
          it.next();
          QString n = it.fileInfo().filePath();
          bool dir = it.fileInfo().isDir();
          row = file_list_model_->AddData(n.toStdString(), dir);
          if (row >= 0) {
            file_table_view_->clearSelection();
            file_table_view_->selectRow(row);
            file_table_view_->setFocus();
            file_table_view_->resizeColumnsToContents();
            launchSelected();
          }
        }
      }
    }
  }
}

// Allow enter if there is a URL
void FileManager::dragEnterEvent(QDragEnterEvent* event) {
  if (event->mimeData()->hasUrls()) {
    event->acceptProposedAction();
  }
}
void FileManager::dropEvent(QDropEvent* event) {
  // Get the drop event info
  addUrls(event->mimeData()->urls());
}

void FileManager::LoadFile(const std::string& file_name) {
  int row = file_list_model_->AddData(file_name, false);
  if (row >= 0) {
    file_table_view_->clearSelection();
    file_table_view_->selectRow(row);
    file_table_view_->setFocus();
    file_table_view_->resizeColumnsToContents();
    launch();
  }
}

void FileManager::LoadFile() {
  QFileDialog dialog(this);
  dialog.setFileMode(QFileDialog::ExistingFile);
  dialog.setNameFilter("CSV file (*.csv)");
  QString file_name = dialog.getOpenFileName(this, "Load a csv file", "C://");

  int row = file_list_model_->AddData(file_name.toStdString(), false);
  if (row >= 0) {
    file_table_view_->clearSelection();
    file_table_view_->selectRow(row);
    file_table_view_->setFocus();
    file_table_view_->resizeColumnsToContents();
    launchSelected();
  }
}

void FileManager::RemoveFile(string file_name) {
  int row = file_list_model_->GetDataRowIndex(file_name);
  file_list_model_->RemoveData(row, row);
  UpdateStatsChart();
  ws.erase(ws.begin() + row);
}

void FileManager::InitializeStatsChart() {
  stats_chart_view_ = ui_.centralwidget->findChild<QChartView*>("StatsChart");
  stats_chart_ = new QChart();
  series_ = new QBarSeries();
  axis_x_ = new QBarCategoryAxis();
  axis_y_ = new QValueAxis();
  stats_chart_->setTitle("Stats Summary");
  stats_chart_->setTheme(QChart::ChartThemeLight);
  stats_chart_->setAnimationOptions(QChart::NoAnimation);

  categories_ << "Avg"
              << "99th pct."
              << "95th pct."
              << "90th pct.";

  axis_x_->append(categories_);
  stats_chart_->addAxis(axis_x_, Qt::AlignBottom);
}

void FileManager::ResetStatsChart() {
  series_->clear();
  // stats_chart_->removeAllSeries();
  sets_.clear();
}

void FileManager::UpdateStatsChart() {
  ResetStatsChart();

  float max_y = std::numeric_limits<float>::min();

  int count = 0;
  for (auto data_model : file_list_model_->data_models_) {
    QBarSet* set = new QBarSet(data_model->fname_);

    *set << data_model->stats.avg << data_model->stats._99thpercentile
         << data_model->stats._95thpercentile
         << data_model->stats._90thpercentile;
    set->setColor(kBarSetColors[count]);
    sets_.push_back(set);
    series_->append(set);

    max_y = std::max(max_y, data_model->stats.avg);
    max_y = std::max(max_y, data_model->stats._99thpercentile);
    max_y = std::max(max_y, data_model->stats._95thpercentile);
    max_y = std::max(max_y, data_model->stats._90thpercentile);
    count++;
  }

  stats_chart_->addSeries(series_);
  series_->attachAxis(axis_x_);

  axis_y_->setRange(0, max_y);
  stats_chart_->addAxis(axis_y_, Qt::AlignLeft);
  series_->attachAxis(axis_y_);
  stats_chart_->legend()->setVisible(true);
  stats_chart_->legend()->setAlignment(Qt::AlignBottom);
  stats_chart_view_->setChart(stats_chart_);
  stats_chart_view_->setRenderHint(QPainter::Antialiasing);
  stats_chart_view_->adjustSize();
  stats_chart_view_->repaint();
}

void FileManager::OnSelectionChange(const QItemSelection& selected,
                                    const QItemSelection& deselected) {
  QModelIndexList selection = selected.indexes();

  for (int i = 0; i < selection.count(); i++) {
    QModelIndex index = selection.at(i);
    std::shared_ptr<DataModel> d = file_list_model_->dataModel(index.row());
    for (auto pc : ws) {
      if (pc->GetFrameDataModel().data_models_[0]->fname_.toStdString().compare(
              d->fname_.toStdString()) == 0) {
        pc->setFocus();
        return;
      }
    }
  }
}