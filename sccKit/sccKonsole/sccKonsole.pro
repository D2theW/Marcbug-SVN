TEMPLATE = app
INCLUDEPATH += include/ \
    ../sccCommon/include/ \
    ../sccGui/include/ \
    ../sccApi/include/
TARGET = sccKonsole
DESTDIR = bin/
QMAKE_CXXFLAGS += -fno-strict-aliasing
QT += core \
    network
HEADERS += include/sccKonsole.h \
    ../sccCommon/include/sccKitVersion.h \
    ../sccCommon/include/coreMask.h \
    ../sccCommon//include/messageHandler.h \
    ../sccCommon/include/sccProgressbar.h\
    ../sccCommon/include/asciiProgress.h \
    ../sccApi/include/sccApi.h \
    ../sccGui/include/sccExtApi.h
SOURCES += src/sccKonsole.cpp \
    ../sccCommon/src/coreMask.cpp \
    src/main.cpp \
    ../sccGui/src/sccExtApi.cpp \
    ../sccApi/src/sccApi.cpp \
    ../sccCommon/src/messageHandler.cpp \
    ../sccCommon/src/sccProgressbar.cpp \
    ../sccCommon/src/asciiProgress.cpp
FORMS += 
RESOURCES += 
