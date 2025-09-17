TEMPLATE = app
TARGET = sccCommonTest
DESTDIR = bin/
INCLUDEPATH += include/ \
    ../sccApi/include
QMAKE_CXXFLAGS += -fno-strict-aliasing
QT += core \
    gui
HEADERS += include/sccDefines.h \
    ../sccCommon/include/sccKitVersion.h \
    include/asciiProgress.h \
    include/sccProgressBar.h \
    include/coreMask.h \
    include/coreMaskDialog.h \
    include/messageHandler.h
SOURCES += src/main.cpp \
    src/asciiProgress.cpp \
    src/sccProgressBar.cpp \
    src/coreMask.cpp \
    src/coreMaskDialog.cpp \
    src/messageHandler.cpp
FORMS += 
RESOURCES += 
