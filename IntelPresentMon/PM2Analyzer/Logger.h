#pragma once
#include <iostream>
#include <fstream>

#ifndef NDEBUG
#define LogMessage(msg) Utilities::Logger::Log(__FILE__,__LINE__, msg)
#else
#define LogMessage(msg) {}
#endif

std::string PiName(void);


namespace Utilities {

    // A singleton logger class
    class Logger {

    public:

        // Delete the copy constructor and assignment
        Logger(const Logger&) = delete;
        void operator= (const Logger&) = delete;

        static Logger& Get() {
            static Logger instance;
            return instance;
        }

        static void Log(const char* file, const int line, const std::string& msg) { return Get().LogImpl(file, line, msg); }
        ~Logger() {
            logFile.close();
        };


    private:

        Logger() {
            // Open Log
            logFile.open(logName);
        };

        void LogImpl(const char* file, const int line, const std::string& msg);

        std::ofstream logFile;
        std::string logName = "Logfile.txt";;
    };

}