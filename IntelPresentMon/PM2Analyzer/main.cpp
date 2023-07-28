#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QtWidgets/QApplication>
#include <iostream>
#include <sstream>

#include "FileManager.h"
#include "Logger.h"
#include "PerfCheck.h"
#include "PerfCheckVersion.h"

static const float kWindowsDesktopRatio = 0.6;

int main(int argc, char *argv[]) {
  // report version
  std::stringstream oss;

  oss << argv[0] << " Version " << PerfCheck_VERSION_MAJOR << "."
      << PerfCheck_VERSION_MINOR;
  LogMessage(oss.str());

  LogMessage("App Started.");
  QApplication app(argc, argv);
  QApplication::setApplicationName("PM2Analyzer");
  QApplication::setApplicationVersion("1.0");

  QCommandLineParser parser;
  parser.setApplicationDescription("Helper");
  parser.addHelpOption();
  parser.addVersionOption();

  QCommandLineOption file_option(
      QStringList() << "f"
                    << "file",
      QCoreApplication::translate("main", "Csv file to be loaded on start."),
      QCoreApplication::translate("main", "csv file"));
  parser.addOption(file_option);
  parser.process(app);

  FileManager w;
  w.show();
  if (parser.isSet("file")) {
    std::string file = parser.value("file").toStdString();
    w.LoadFile(file);
  }

  return app.exec();
}