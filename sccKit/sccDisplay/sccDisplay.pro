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
HEADERS += \
    ../sccCommon/include/sccKitVersion.h \
    include/sccTilePreview.h \
    include/sccVirtualDisplay.h \
    include/sccDisplayWidget.h \
    include/sccDisplay.h \
    ../sccGui/include/perfmeterWidget.h \
    ../sccCommon/include/messageHandler.h \
    ../sccCommon/include/sccProgressbar.h\
    ../sccCommon/include/asciiProgress.h \
    ../sccCommon/include/coreMask.h \
    ../sccApi/include/sccApi.h \
    ../sccGui/include/sccExtApi.h
SOURCES += \
    src/sccTilePreview.cpp \
    src/sccVirtualDisplay.cpp \
    src/sccDisplayWidget.cpp \
    src/sccDisplay.cpp \
    src/main.cpp \
    ../sccGui/src/perfmeterWidget.cpp \
    ../sccCommon/src/messageHandler.cpp \
    ../sccCommon/src/sccProgressbar.cpp \
    ../sccCommon/src/asciiProgress.cpp \
    ../sccCommon/src/coreMask.cpp \
    ../sccApi/src/sccApi.cpp \
    ../sccGui/src/sccExtApi.cpp
