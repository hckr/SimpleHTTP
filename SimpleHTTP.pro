TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

LIBS += -lws2_32

CONFIG += c++11

SOURCES += main.cpp \
    serverlistener.cpp \
    requestparser.cpp

include(deployment.pri)
qtcAddDeployment()

HEADERS += \
    serverlistener.h \
    requestparser.h \
    serverexceptions.h

