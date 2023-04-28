#pragma once

#include <iostream>
#include <fstream>
#include <QtWidgets/QTextEdit>


#ifndef NDEBUG
#define InfoMessage(msg) myMSG::InfoMsg(__FILE__,__LINE__, msg)
#else
#define InfoMessage(msg) {}
#endif


class myMSG {
public:

    // Delete the copy constructor and assignment
    myMSG(const myMSG&) = delete;
    void operator= (const myMSG&) = delete;


    static myMSG& Get() {
        static myMSG instance;
        return instance;
    }

    static void InfoMsg(const char* file, const int line, const std::string& msg) {
        return Get().MSGImpl(file, line, msg);
    }

    static void setMessagePanel(QTextEdit* msgptr) {
        Get().msgPtr = msgptr;
    }

    ~myMSG() {
    };


private:


    myMSG() {
        // Open Log
        // Connect to the msgpanel

    };

    QTextEdit* msgPtr = nullptr;

    void MSGImpl(const char* file, const int line, const std::string& msg);

};

