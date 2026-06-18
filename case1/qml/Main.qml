import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

ApplicationWindow {
    id: rootWindow
    visible: true
    width: 1024
    height: 720
    title: "Анализ дефектов дорожного покрытия"
    color: "#000000" 

    property string playState: "inactive" 
    property int frameCounter: 0
    property int totalFrames: 1
    property int currentFrame: 0

    footer: Rectangle {
        height: 30
        color: "#000000"
        border.color: "#1e1e2f"
        border.width: 1
        Text {
            id: statusBarText
            text: "Готов"
            color: "#ffffff"
            font.pixelSize: 11
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: 10
        }
    }

    Connections {
        target: videoBackend
        
        function onVideoEnded() { playState = "ended" }
        function onStatusMessage(msg) { statusBarText.text = msg }
        function onFrameReady(frame) {
            frameCounter++
            videoImage.source = "image://video/frame_" + frameCounter
        }
        function onVideoOpened(total) { totalFrames = total }
        function onFramePositionChanged(current) { currentFrame = current }

        function onStatsUpdated(currCrit, currCracks, currWear, currInfra, accCrit, accCracks, accWear, accInfra, frames) {
            
            let totalAcc = accCrit + accCracks + accWear + accInfra;
            let pCrit = totalAcc > 0 ? ((accCrit / totalAcc) * 100).toFixed(1) : "0.0";
            let pCracks = totalAcc > 0 ? ((accCracks / totalAcc) * 100).toFixed(1) : "0.0";
            let pWear = totalAcc > 0 ? ((accWear / totalAcc) * 100).toFixed(1) : "0.0";
            let pInfra = totalAcc > 0 ? ((accInfra / totalAcc) * 100).toFixed(1) : "0.0";

            statsTextArea.text = 
                "===================================\n" +
                "  В ТЕКУЩЕМ КАДРЕ\n" +
                "===================================\n" +
                " Критические (Ямы):   " + currCrit + "\n" +
                " Трещины полотна:     " + currCracks + "\n" +
                " Износ / Разметка:    " + currWear + "\n" +
                " Инфраструктура:      " + currInfra + "\n\n" +
                "===================================\n" +
                "  РАСПРЕДЕЛЕНИЕ ДЕФЕКТОВ\n" +
                "===================================\n" +
                " Всего найдено:       " + totalAcc + "\n" +
                " Критические:         " + accCrit + " (" + pCrit + "%)\n" +
                " Трещины:             " + accCracks + " (" + pCracks + "%)\n" +
                " Износ:               " + accWear + " (" + pWear + "%)\n" +
                " Инфраструктура:      " + accInfra + " (" + pInfra + "%)\n" +
                "-----------------------------------\n" +
                " ОБРАБОТАНО КАДРОВ:   " + frames + "\n" +
                "===================================\n"
        }
    }

    FileDialog {
        id: videoDialog
        title: "Выберите видеофайл"
        nameFilters: ["Видео файлы (*.mp4 *.avi *.mkv)"]
        onAccepted: {
            frameCounter = 0
            playState = "playing"
            videoBackend.loadVideo(videoDialog.currentFile)
        }
    }

    FileDialog {
        id: photoDialog
        title: "Выберите фото"
        nameFilters: ["Изображения (*.jpg *.png *.bmp)"]
        onAccepted: {
            frameCounter = 0
            playState = "inactive"
            videoBackend.loadPhoto(photoDialog.currentFile)
        }
    }

    FileDialog {
        id: modelDialog
        title: "Выберите файл модели YOLO"
        nameFilters: ["ONNX модели (*.onnx)"]
        onAccepted: videoBackend.loadModel(modelDialog.currentFile)
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "#000000"

                Image {
                    id: videoImage
                    anchors.fill: parent
                    anchors.margins: 10
                    fillMode: Image.PreserveAspectFit
                    source: "image://video/frame_0"
                    cache: false
                }
            }

            Rectangle {
                Layout.fillWidth: true
                height: 120 
                color: "#000000"
                border.color: "#1e1e2f"
                border.width: 1

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12
                        visible: playState !== "inactive"
                        
                        Text { text: "Кадр"; color: "white"; font.bold: true; font.pixelSize: 11 }
                        
                        WavySlider {
                            id: videoSeeker
                            totalFrames: rootWindow.totalFrames
                            currentFrame: rootWindow.currentFrame
                            playState: rootWindow.playState
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 20

                        PlaybackButton {
                            id: playbackBtn
                            stateValue: playState

                            onClicked: {
                                if (playState === "playing") {
                                    videoBackend.pauseVideo()
                                    playState = "paused"
                                } else if (playState === "paused") {
                                    videoBackend.resumeVideo()
                                    playState = "playing"
                                } else if (playState === "ended") {
                                    videoBackend.restartVideo()
                                    playState = "playing"
                                }
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4

                            Text {
                                text: "Порог уверенности: " + (confSlider.value / 100.0).toFixed(2)
                                color: "white"
                                font.bold: true
                                font.pixelSize: 12
                            }

                            Slider {
                                id: confSlider
                                from: 10; to: 90; value: 25; stepSize: 1
                                Layout.fillWidth: true

                                background: Rectangle {
                                    x: confSlider.leftPadding
                                    y: confSlider.topPadding + confSlider.availableHeight / 2 - height / 2
                                    implicitWidth: 200; implicitHeight: 4
                                    width: confSlider.availableWidth; height: implicitHeight
                                    radius: 2; color: "#313244"
                                    Rectangle { width: confSlider.visualPosition * parent.width; height: parent.height; color: "#ff8c00"; radius: 2 }
                                }

                                handle: Rectangle {
                                    x: confSlider.leftPadding + confSlider.visualPosition * (confSlider.availableWidth - width)
                                    y: confSlider.topPadding + confSlider.availableHeight / 2 - height / 2
                                    implicitWidth: 16; implicitHeight: 16
                                    radius: 8; color: "white"; border.color: "#ff8c00"; border.width: 1
                                }

                                onValueChanged: videoBackend.setConfidenceThreshold(value / 100.0)
                            }
                        }
                    }
                }
            }
        }

        Rectangle { width: 1; Layout.fillHeight: true; color: "#1e1e2f" }

        Rectangle {
            width: 320
            Layout.fillHeight: true
            color: "#000000"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 15
                spacing: 12

                Text { text: "ПАНЕЛЬ УПРАВЛЕНИЯ"; color: "white"; font.bold: true; font.pixelSize: 13; Layout.alignment: Qt.AlignLeft }

                GridLayout {
                    columns: 2; rowSpacing: 8; columnSpacing: 8; Layout.fillWidth: true

                    CustomButton { text: "Модель (.onnx)"; onClicked: modelDialog.open() }
                    CustomButton { text: "Фото (.png)"; onClicked: photoDialog.open() }
                    CustomButton { text: "Видео (.mp4)"; onClicked: videoDialog.open() }
                    CustomButton { text: "Камера (Live)"; onClicked: videoBackend.startCamera() }
                }

                Rectangle { height: 1; Layout.fillWidth: true; color: "#1e1e2f" }

                ColumnLayout {
                    spacing: 8
                    Layout.fillWidth: true
                    
                    Text { 
                        text: "ЛЕГЕНДА КРИТИЧНОСТИ"
                        color: "#a6adc8"
                        font.bold: true
                        font.pixelSize: 10 
                    }

                    RowLayout {
                        spacing: 8
                        Rectangle {
                            width: 10
                            height: 10
                            radius: 5
                            color: "#ff0000"
                            antialiasing: true
                        }
                        Text {
                            text: "Критические дефекты (Ямы D20)"
                            color: "white"
                            font.pixelSize: 10
                        }
                    }
                    
                    RowLayout {
                        spacing: 8
                        Rectangle {
                            width: 10
                            height: 10
                            radius: 5
                            color: "#ff8c00"
                            antialiasing: true
                        }
                        Text {
                            text: "Трещины полотна (D00, D01)"
                            color: "white"
                            font.pixelSize: 10
                        }
                    }
                    
                    RowLayout {
                        spacing: 8
                        Rectangle {
                            width: 10
                            height: 10
                            radius: 5
                            color: "#ffff00"
                            antialiasing: true
                        }
                        Text {
                            text: "Износ (Разметка, колея)"
                            color: "white"
                            font.pixelSize: 10
                        }
                    }
                    
                    RowLayout {
                        spacing: 8
                        Rectangle {
                            width: 10
                            height: 10
                            radius: 5
                            color: "#0000ff"
                            antialiasing: true
                        }
                        Text {
                            text: "Инфраструктура (Люки, Заплатки)"
                            color: "white"
                            font.pixelSize: 10
                        }
                    }
                }

                Rectangle { height: 1; Layout.fillWidth: true; color: "#1e1e2f" }

                Text { text: "ПАРАМЕТРЫ СТАТИСТИКИ"; color: "white"; font.bold: true; font.pixelSize: 11 }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: "#0c0c0e"
                    border.color: "#1e1e2f"
                    border.width: 1
                    radius: 6
                    
                    ScrollView {
                        anchors.fill: parent
                        anchors.margins: 8
                        clip: true
                        
                        TextArea { 
                            id: statsTextArea
                            readOnly: true
                            color: "#a6adc8"
                            font.family: "Monospace"
                            font.pixelSize: 11
                            text: "===================================\n  СТАТИСТИКА ДЕФЕКТОВ\n===================================\nОжидание анализа...\n"
                            wrapMode: TextArea.Wrap
                            background: null 
                        }
                    }
                }

                RowLayout {
                    spacing: 8
                    Layout.fillWidth: true
                    
                    CustomButton { text: "Сбросить"; onClicked: videoBackend.resetStats() }
                    CustomButton { text: "Экспорт в CSV"; onClicked: videoBackend.exportCSV() }
                }
            }
        }
    }
}
