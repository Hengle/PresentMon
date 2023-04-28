#pragma once
#include <vector>
#include <string>

typedef std::vector<std::string> Row;

class CSVParser
{
public:
    CSVParser();
    ~CSVParser();

    bool Parse(const std::string filename, std::vector<Row>& result);
};
