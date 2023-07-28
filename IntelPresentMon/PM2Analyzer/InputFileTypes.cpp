#include "InputFileTypes.h"

#include <qdebug.h>

#include <QMessageBox>
#include <QtCharts/QLineSeries>
#include <iostream>
#include <memory>

#include "CSVParser.h"
#include "Parsers.h"

InputFileTypesProcessor::InputFileTypesProcessor(QObject* parent) {}

InputFileTypesProcessor::~InputFileTypesProcessor() {}

InputFileTypesProcessor::inputSig InputFileTypesProcessor::Parse(
    const std::string& fileName, std::vector<Row>& rows,
    std::vector<std::shared_ptr<Frame>>& frames) {
  // Check for input file sigs
  if (FrapsParser::SigMatch(rows[0])) {
    std::unique_ptr<Parser> parser = std::make_unique<FrapsParser>();
    rows.erase(rows.begin());
    if (parser->ParseRows(fileName, rows, frames)) return parser->type();
  } else if (NewPresentMonParser::SigMatch(rows[0])) {
    std::unique_ptr<Parser> parser = std::make_unique<NewPresentMonParser>();
    if (parser->ParseRows(fileName, rows, frames)) return parser->type();
  } else if (PresentMonParser::SigMatch(rows[0])) {
    std::unique_ptr<Parser> parser = std::make_unique<PresentMonParser>();
    if (parser->ParseRows(fileName, rows, frames)) return parser->type();
  } else if (PresentMon2Parser::SigMatch(rows[0])) {
    std::unique_ptr<Parser> parser = std::make_unique<PresentMon2Parser>();
    if (parser->ParseRows(fileName, rows, frames)) return parser->type();
  } else {
    auto msg_box = std::make_unique<QMessageBox>();
    msg_box->setModal(true);
    msg_box->setText("Invalid present mon data.");
    msg_box->setIcon(QMessageBox::Warning);
    msg_box->setStandardButtons(QMessageBox::Ok);

    msg_box->setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    msg_box->setStyleSheet("background-color:#444;color:#FFF;outline:none;");
    msg_box->exec();
  }

  return InputFileTypesProcessor::Error;
}
