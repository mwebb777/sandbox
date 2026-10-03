QT = core network gui httpserver websockets

CONFIG += c++17 console

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

linux-g++: {
        # Include paths for OpenCV headers
        INCLUDEPATH += /usr/include/opencv4

        # Link required OpenCV libraries
        LIBS += -L/usr/lib \
                -lopencv_core \
                -lopencv_imgproc \
                -lopencv_highgui \
                -lopencv_imgcodecs \
                -lopencv_videoio
}

win32: {
        INCLUDEPATH += d:\cots\opencv\include

        LIBS += -Ld:\cots\opencv\x64\vc16\lib \
                -lopencv_world4100d
}

SOURCES += \
        main.cpp



# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
