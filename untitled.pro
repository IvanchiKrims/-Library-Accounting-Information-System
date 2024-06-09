QT += core gui sql

QT       += sql
INCLUDEPATH += C:\Qt\Docs\Qt-6.6.1\qtsql
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

TARGET = YourProjectName
TEMPLATE = app

SOURCES += main.cpp \
    mainwindow.cpp

HEADERS += mainwindow.h

FORMS += mainwindow.ui

