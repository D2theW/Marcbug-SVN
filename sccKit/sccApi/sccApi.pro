TEMPLATE = app
TARGET = sccApiTest
DESTDIR = bin/
INCLUDEPATH += include/ \
    ../sccCommon/include
QMAKE_CXXFLAGS += -fno-strict-aliasing
QT += core network
HEADERS += include/sccApi.h \
    ../sccCommon/include/sccKitVersion.h \
    ../sccCommon/include/messageHandler.h \
    ../sccCommon/include/sccProgressbar.h\
    ../sccCommon/include/asciiProgress.h
SOURCES += src/main.cpp \
    src/sccApi.cpp \
    ../sccCommon/src/messageHandler.cpp \
    ../sccCommon/src/sccProgressbar.cpp \
    ../sccCommon/src/asciiProgress.cpp
FORMS += 
RESOURCES += 
