#pragma once
#include <QtWidgets/QApplication>
#include <QtCharts/QLineSeries>

#define QT_CHARTS_USE_NAMESPACE


class Series : public QLineSeries {
public:
    Series(QObject* parent = nullptr) :
        QLineSeries(parent)
    {
        connect(this, &QXYSeries::clicked, this, &Series::onClicked);
    }

private slots:
    void onClicked();
};
