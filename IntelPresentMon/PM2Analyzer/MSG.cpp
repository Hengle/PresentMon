#include "MSG.h"

#include <QtWidgets/QTextEdit>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>

void myMSG::MSGImpl(const char* file, const int line, const std::string& msg) {
#ifdef WIN32
  char drive[10], dir[100], fname[100], ext[10];
  ;
  _splitpath_s(file, drive, dir, fname, ext);
#else
  char* fname = basename((char*)file);

#endif

  time_t rawtime;
  struct tm current_time;
  time(&rawtime);
  errno_t result = localtime_s(&current_time, &rawtime);

  char buffer[1024];
  asctime_s(buffer, &current_time);

  std::stringstream st;
  st << "Time : ";
  st << std::setfill('0') << std::setw(2);
  st << buffer;

  std::stringstream oss;
  oss << st.str() << "    ";
  oss << "[File] ";
  oss << std::left << std::setw(20);
  oss << fname;
  oss << "[Line] ";
  oss << std::right << std::setw(8);
  oss << line;
  oss << "  [Msg] " << msg;

  // Update text in panel
  if (msgPtr != nullptr) msgPtr->append(QString(msg.c_str()));
}