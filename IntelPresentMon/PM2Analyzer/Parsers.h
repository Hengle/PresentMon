#pragma once
//#include <QtCharts/QLineSeries>
#include <QObject>
#include <QProgressDialog>
#include <map>
#include <memory>

#include "CSVParser.h"
#include "Frame.h"
#include "InputFileTypes.h"

// Base class for parsers
class Parser {
   public:
    virtual bool ParseRows(const std::string& filename,
                           const std::vector<Row>& rows,
                           std::vector<std::shared_ptr<Frame>>& frames) = 0;
    virtual InputFileTypesProcessor::inputSig type() = 0;
};

// Base class for parsers
class FrapsParser : public Parser {
   public:
    bool static SigMatch(const Row& r);
    bool ParseRows(const std::string& fileName, const std::vector<Row>& rows,
                   std::vector<std::shared_ptr<Frame>>& frames) override;
    bool ParseLine(const Row& r, std::shared_ptr<Frame>& frame);
    InputFileTypesProcessor::inputSig type() override {
        return InputFileTypesProcessor::FRAPS;
    }

   private:
    qreal parser_last_time_ = 0;
    bool first_pass_ = true;
};

// Base class for parsers
class PresentMonParser : public Parser {
   public:
    PresentMonParser() : canceled_(false){};
    bool static SigMatch(const Row& r);

    bool ParseRows(const std::string& fileName, const std::vector<Row>& rows,
                   std::vector<std::shared_ptr<Frame>>& frames) override;
    bool ParseLine(const Row& r, std::shared_ptr<Frame>& frame);
    InputFileTypesProcessor::inputSig type() override {
        return InputFileTypesProcessor::PresentMon;
    }

    void Cancel();

    void NormalizeMetrics(std::shared_ptr<Frame> frame);

   private:
    qreal parser_last_time_ = 0;
    bool first_pass_ = true;
    std::map<std::string, int> field_index_;
    bool canceled_;
};

class PresentMon2Parser : public Parser, public QObject {
   public:
    PresentMon2Parser() : canceled_(false) {
        progress_ = new QProgressDialog();
        QObject::connect(progress_, &QProgressDialog::canceled, this,
                         [this]() { this->Cancel(); });
    };

    ~PresentMon2Parser() {
        if (progress_ != nullptr) {
            delete progress_;
        }
    }
    bool static SigMatch(const Row& r);

    bool ParseRows(const std::string& fileName, const std::vector<Row>& rows,
                   std::vector<std::shared_ptr<Frame>>& frames) override;
    bool ParseLine(const Row& r, std::shared_ptr<Frame>& frame);
    InputFileTypesProcessor::inputSig type() override {
        return InputFileTypesProcessor::PresentMon;
    }

    void Cancel();

    void NormalizeMetrics(std::shared_ptr<Frame> frame);

   private:
    qreal parser_last_time_ = 0;
    bool first_pass_ = true;
    std::map<std::string, int> field_index_;
    bool canceled_;
    QProgressDialog* progress_;
};

// Base class for parsers
class NewPresentMonParser : public Parser {
   public:
    bool static SigMatch(const Row& r);

    bool ParseRows(const std::string& fileName, const std::vector<Row>& rows,
                   std::vector<std::shared_ptr<Frame>>& frames) override;
    bool ParseLine(const Row& r, std::shared_ptr<Frame>& frame);
    InputFileTypesProcessor::inputSig type() override {
        return InputFileTypesProcessor::NewPresentMon;
    }

    void NormalizeMetrics(std::shared_ptr<Frame> frame);

   private:
    qreal parser_last_time_ = 0;
    bool first_pass_ = true;
    std::map<std::string, int> field_index_;
};
