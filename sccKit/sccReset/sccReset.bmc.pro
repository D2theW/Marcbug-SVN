TEMPLATE = app
INCLUDEPATH += include/ \
    ../sccCommon/include/ \
    ../sccApi/include/ \
    ../sccGui/include/
TARGET = sccReset
DESTDIR = bin/
QMAKE_CXXFLAGS += -fno-strict-aliasing -DBMC_IS_HOST
QT += core \
    network
HEADERS += include/sccReset.h \
    ../sccCommon/include/sccKitVersion.h \
    ../sccCommon/include/coreMask.h \
    ../sccGui/include/sccExtApi.h \
    ../sccCommon//include/messageHandler.h \
    ../sccCommon/include/sccProgressbar.h\
    ../sccCommon/include/asciiProgress.h \
    ../sccApi/include/sccApi.h
SOURCES += src/sccReset.cpp \
    ../sccCommon/src/coreMask.cpp \
    src/main.cpp \
    ../sccGui/src/sccExtApi.cpp \
    ../sccCommon/src/messageHandler.cpp \
    ../sccCommon/src/sccProgressbar.cpp \
    ../sccCommon/src/asciiProgress.cpp \
    ../sccApi/src/sccApi.cpp
FORMS += 
RESOURCES += 
