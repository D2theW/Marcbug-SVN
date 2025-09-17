TEMPLATE = app
INCLUDEPATH += include/ \
    ../sccCommon/include/ \
    ../sccApi/include/
DESTDIR = bin/
QMAKE_CXXFLAGS += -fno-strict-aliasing
QT += core \
    network
HEADERS += ../sccCommon/include/sccKitVersion.h \
    include/sccDump.h \
    ../sccCommon/include/coreMask.h \
    ../sccCommon/include/messageHandler.h \
    ../sccCommon/include/sccProgressbar.h\
    ../sccCommon/include/asciiProgress.h \
    ../sccApi/include/sccApi.h
SOURCES += src/sccDump.cpp \
    src/main.cpp \
    ../sccCommon/src/coreMask.cpp \
    ../sccCommon/src/messageHandler.cpp \
    ../sccCommon/src/sccProgressbar.cpp \
    ../sccCommon/src/asciiProgress.cpp \
    ../sccApi/src/sccApi.cpp
    
