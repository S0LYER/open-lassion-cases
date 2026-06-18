cmake_minimum_required(VERSION 3.16)
project(SafetyMonitor VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick Widgets)
find_package(OpenCV REQUIRED COMPONENTS core imgproc videoio imgcodecs dnn)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

qt_add_executable(appSafetyMonitor
    src/main.cpp
    src/videoitem.h
    src/videobackend.h
    src/videobackend.cpp
)

qt_add_qml_module(appSafetyMonitor
    URI App
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/CustomButton.qml
        qml/PlaybackButton.qml
        qml/WavySlider.qml
)

target_include_directories(appSafetyMonitor PRIVATE src)
target_link_libraries(appSafetyMonitor PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Widgets ${OpenCV_LIBS})
