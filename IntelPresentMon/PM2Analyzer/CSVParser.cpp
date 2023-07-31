#include "CSVParser.h"

#include <fstream>
#include <iostream>
#include <sstream>

#include "Logger.h"
#include "MSG.h"

CSVParser::CSVParser() {}

CSVParser::~CSVParser() {}

class WordDelimitedByCommas : public std::string {};

std::istream& operator>>(std::istream& is, WordDelimitedByCommas& output) {
  std::getline(is, output, ',');
  return is;
}

bool CSVParser::Parse(const std::string filename, std::vector<Row>& result) {
  InfoMessage("Loading : " + filename);

  // read example file
  std::ifstream infile;
  infile.open(filename);

  if (infile.is_open()) {
    std::string line;

    while (std::getline(infile, line)) {
      std::istringstream iss(line);
      Row tokens((std::istream_iterator<WordDelimitedByCommas>(iss)),
                 std::istream_iterator<WordDelimitedByCommas>());

      result.push_back(tokens);
    }

    infile.close();
    InfoMessage("Finished " + filename + " parsing.");
    return true;
  } else {
    InfoMessage("Failed Opening : " + filename + " file for reading.");
    return false;
  }
}
