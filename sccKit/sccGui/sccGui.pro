TEMPLATE = app
INCLUDEPATH += include/ \
    ../sccCommon/include/ \
    ../sccApi/include/
DESTDIR = bin/
QMAKE_CXXFLAGS += -fno-strict-aliasing
QT += core \
    gui \
    network
HEADERS += ../sccCommon/include/sccKitVersion.h \
    include/perfmeterWidget.h \
    include/flitWidget.h \
    include/packetWidget.h \
    include/readMemWidget.h \
    include/sccExtApi.h \
    include/sccGui.h \
    ../sccCommon/include/coreMask.h \
    ../sccCommon/include/coreMaskDialog.h \
    ../sccCommon/include/messageHandler.h \
    ../sccCommon/include/sccProgressbar.h\
    ../sccCommon/include/asciiProgress.h \
    ../sccApi/include/sccApi.h
SOURCES += src/perfmeterWidget.cpp \
    src/flitWidget.cpp \
    src/packetWidget.cpp \
    src/readMemWidget.cpp \
    src/sccExtApi.cpp \
    src/sccGui.cpp \
    src/main.cpp \
    ../sccCommon/src/coreMask.cpp \
    ../sccCommon/src/coreMaskDialog.cpp \
    ../sccCommon/src/messageHandler.cpp \
    ../sccCommon/src/sccProgressbar.cpp \
    ../sccCommon/src/asciiProgress.cpp \
    ../sccApi/src/sccApi.cpp
FORMS += ui/readMemWidget.ui \
    ui/flitWidget.ui \
    ui/packetWidget.ui \
    ui/sccGui.ui
RESOURCES = ui/resources.qrc
