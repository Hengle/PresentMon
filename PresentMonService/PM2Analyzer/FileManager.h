#pragma once
#include <QBarCategoryAxis>
#include <QBarSeries>
#include <QBarSet>
#include <QChart>
#include <QChartView>
#include <QValueAxis>
#include <QtWidgets/QMainWindow>
#include <vector>

#include "FrameDataModel.h"
#include "ui_FileManager.h"

class PerfCheck;
class PerfView;

class FileManager : public QMainWindow {
  Q_OBJECT

 public:
  FileManager(QWidget* parent = Q_NULLPTR);

 public slots:
  void LoadFile();
  void LoadFile(const std::string& file_name);
  void RemoveFile(string file_name);
  void dataChanged(QModelIndex top_left, QModelIndex bottom_right) {
    file_table_view_->resizeColumnsToContents();
    file_table_view_->horizontalHeader()->setSectionResizeMode(
        QHeaderView::Stretch);
  };
  void OnSelectionChange(const QItemSelection& selected,
                         const QItemSelection& deselected);
  void UpdateStatsChart();

 private:
  void InitializeStatsChart();
  void ResetStatsChart();

  Ui::FileManagerClass ui_;
  QMdiArea* mdi_area_;
  QTableView* file_table_view_;
  QAction* load_action_;
  QAction* export_chart_action_;
  QAction* export_stats_graph_action_;
  QAction* export_stats_table_action_;

  // Stats chart
  QChartView* stats_chart_view_;
  QChart* stats_chart_;
  std::vector<QBarSet*> sets_;
  QBarSeries* series_;
  QBarCategoryAxis* axis_x_;
  QValueAxis* axis_y_;
  QStringList categories_;

  // Current file list model
  std::unique_ptr<FrameDataModel> file_list_model_;

  // Open windows for plots
  std::vector<PerfCheck*> ws;

  void addUrls(const QList<QUrl>& urlList);
  void WriteSettings();
  void ReadSettings();
  void dragEnterEvent(QDragEnterEvent* event) override;
  void dropEvent(QDropEvent* event) override;

 private slots:
  void PrintChart();
  void PrintStatsTable();
  void PrintStatsSummary();
  void launch();
  void launchSelected();
  void kill();
};
