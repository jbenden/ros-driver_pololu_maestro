TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

INCLUDEPATH += \
    /opt/ros/melodic/include

SOURCES += \
    src/driver.cpp

DISTFILES += \
    CMakeLists.txt \
    package.xml

HEADERS += \
    src/driver.h
