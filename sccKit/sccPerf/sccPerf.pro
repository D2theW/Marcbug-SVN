TEMPLATE = app
INCLUDEPATH += include/ \
    ../sccCommon/include/ \
    ../sccGui/include/ \
    ../sccApi/include/
DESTDIR = bin/
QMAKE_CXXFLAGS += -fno-strict-aliasing
QT += core \
    gui \
    network
HEADERS += ../sccCommon/include/sccKitVersion.h \
    ../sccGui/include/perfmeterWidget.h \
    include/sccPerf.h \
    ../sccCommon/include/coreMask.h \
    ../sccCommon/include/coreMaskDialog.h \
    ../sccCommon/include/messageHandler.h \
    ../sccCommon/include/sccProgressbar.h\
    ../sccCommon/include/asciiProgress.h \
    ../sccApi/include/sccApi.h \
    ../sccGui/include/sccExtApi.h
SOURCES += ../sccGui/src/perfmeterWidget.cpp \
    src/sccPerf.cpp \
    src/main.cpp \
    ../sccCommon/src/coreMask.cpp \
    ../sccCommon/src/coreMaskDialog.cpp \
    ../sccCommon/src/messageHandler.cpp \
    ../sccCommon/src/sccProgressbar.cpp \
    ../sccCommon/src/asciiProgress.cpp \
    ../sccApi/src/sccApi.cpp \
    ../sccGui/src/sccExtApi.cpp
    
