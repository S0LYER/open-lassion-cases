import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

ApplicationWindow {
    id: rootWindow
    visible: true
    width: 1024
    height: 720
    title: "Детектор пыли и задымления"
    color: "#000000" 

    property string playState: "inactive" 
    property int frameCounter: 0
    property int totalFrames: 1
    property int currentFrame: 0

    footer: Rectangle {
        height: 30; color: "#000000"; border.color: "#1e1e2f"; border.width: 1
        Text {
            id: statusBarText; text: "Готов"
            color: "#ffffff"; font.pixelSize: 11; anchors.verticalCenter: parent.verticalCenter; anchors.left: parent.left; anchors.leftMargin: 10
        }
    }

    Connections {
        target: videoBackend
        onVideoEnded: { playState = "ended" }
        onStatusMessage: (msg) => { statusBarText.text = msg }
        onLogMessage: (msg) => { logTextArea.append(msg) }
        onFrameReady: (frame) => {
            frameCounter++
            videoImage.source = "image://video/frame_" + frameCounter
        }
        onVideoOpened: (total) => { totalFrames = total }
        onFramePositionChanged: (current) => { currentFrame = current }
        
        onAlarmStatusUpdated: (isAlarm, density) => {
            densityText.text = "Локальная плотность пыли: " + density.toFixed(1) + "%"
            if (isAlarm) {
                alarmBanner.color = "#ff0000" 
                alarmLabel.text = "ВНИМАНИЕ: ТРЕВОГА!"
                alarmLabel.color = "white"
            } else {
                alarmBanner.color = "#ffff00" 
                alarmLabel.text = "НОРМА"
                alarmLabel.color = "black"
            }
        }
    }

    FileDialog {
        id: videoDialog
        title: "Выберите видеофайл"
        nameFilters: ["Видео файлы (*.mp4 *.avi *.mkv)"]
        onAccepted: {
            frameCounter = 0
            playState = "paused" 
            videoBackend.loadVideo(videoDialog.currentFile)
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        ColumnLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; spacing: 0

            Rectangle {
                Layout.fillWidth: true; Layout.fillHeight: true; color: "#000000"

                Image {
                    id: videoImage
                    anchors.fill: parent; anchors.margins: 10
                    fillMode: Image.PreserveAspectFit
                    source: "image://video/frame_0"
                    cache: false

                    MouseArea {
                        id: drawArea
                        anchors.fill: parent
                        
                        property int startX: 0
                        property int startY: 0
                        property bool isDrawing: false

                        onPressed: (mouse) => {
                            if (playState !== "playing") { 
                                startX = mouse.x
                                startY = mouse.y
                                isDrawing = true
                                selectionRect.x = mouse.x
                                selectionRect.y = mouse.y
                                selectionRect.width = 0
                                selectionRect.height = 0
                                selectionRect.visible = true
                            }
                        }
                        onPositionChanged: (mouse) => {
                            if (isDrawing) {
                                selectionRect.width = Math.max(1, mouse.x - startX)
                                selectionRect.height = Math.max(1, mouse.y - startY)
                            }
                        }
                        onReleased: {
                            if (isDrawing) {
                                isDrawing = false
                                let imgW = videoImage.paintedWidth
                                let imgH = videoImage.paintedHeight
                                let offX = (videoImage.width - imgW) / 2
                                let offY = (videoImage.height - imgH) / 2

                                let nx = (selectionRect.x - offX) / imgW
                                let ny = (selectionRect.y - offY) / imgH
                                let nw = selectionRect.width / imgW
                                let nh = selectionRect.height / imgH

                                videoBackend.setReferenceArea(nx, ny, nw, nh)
                                selectionRect.visible = false 
                            }
                        }
                    }

                    Rectangle {
                        id: selectionRect
                        visible: false
                        color: "#40ffffff"
                        border.color: "white"
                        border.width: 2
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true; height: 120; color: "#000000"; border.color: "#1e1e2f"; border.width: 1

                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 12; spacing: 8

                    RowLayout {
                        Layout.fillWidth: true; spacing: 12; visible: playState !== "inactive"
                        Text { text: "Кадр"; color: "white"; font.bold: true; font.pixelSize: 11 }
                        WavySlider { id: videoSeeker; totalFrames: rootWindow.totalFrames; currentFrame: rootWindow.currentFrame; playState: rootWindow.playState }
                    }

                    RowLayout {
                        Layout.fillWidth: true; spacing: 20

                        PlaybackButton {
                            id: playbackBtn; stateValue: playState
                            onClicked: {
                                if (playState === "playing") {
                                    videoBackend.pauseVideo(); playState = "paused"
                                } else if (playState === "paused") {
                                    videoBackend.resumeVideo(); playState = "playing"
                                } else if (playState === "ended") {
                                    videoBackend.restartVideo(); playState = "playing"
                                }
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true; spacing: 4
                            Text { text: "Порог тревоги: " + alarmSlider.value + "%"; color: "white"; font.bold: true; font.pixelSize: 12 }
                            Slider {
                                id: alarmSlider
                                from: 5; to: 50; value: 20; stepSize: 1; Layout.fillWidth: true
                                background: Rectangle {
                                    x: alarmSlider.leftPadding; y: alarmSlider.topPadding + alarmSlider.availableHeight / 2 - height / 2
                                    implicitWidth: 200; implicitHeight: 4; width: alarmSlider.availableWidth; height: implicitHeight; radius: 2; color: "#313244"
                                    Rectangle { width: alarmSlider.visualPosition * parent.width; height: parent.height; color: "#ff8c00"; radius: 2 }
                                }
                                handle: Rectangle {
                                    x: alarmSlider.leftPadding + alarmSlider.visualPosition * (alarmSlider.availableWidth - width); y: alarmSlider.topPadding + alarmSlider.availableHeight / 2 - height / 2
                                    implicitWidth: 16; implicitHeight: 16; radius: 8; color: "white"; border.color: "#ff8c00"; border.width: 1
                                }
                                onValueChanged: videoBackend.setAlarmThreshold(value)
                            }
                        }
                    }
                }
            }
        }

        Rectangle { width: 1; Layout.fillHeight: true; color: "#1e1e2f" }

        Rectangle {
            width: 320; Layout.fillHeight: true; color: "#000000"

            ColumnLayout {
                anchors.fill: parent; anchors.margins: 15; spacing: 12

                Text { text: "СИСТЕМА ДЕТЕКЦИИ"; color: "white"; font.bold: true; font.pixelSize: 13 }

                Rectangle {
                    Layout.fillWidth: true; height: 110; color: "#0c0c0e"; border.color: "#ff8c00"; border.width: 1; radius: 6
                    Text {
                        anchors.fill: parent; anchors.margins: 10
                        text: "ИНСТРУКЦИЯ:\n1. Загрузите видео (встанет на паузу).\n2. Зажмите левую кнопку мыши на видео и ВЫДЕЛИТЕ пылевое облако (без техники!).\n3. Нажмите ▶ Плей."
                        color: "#a6adc8"; font.pixelSize: 11; wrapMode: Text.WordWrap
                    }
                }

                CustomButton { text: "Загрузить видео"; onClicked: videoDialog.open() }
                CustomButton { text: "Остановить сброс"; onClicked: { videoBackend.stopVideo(); playState = "inactive" } }

                Rectangle { height: 1; Layout.fillWidth: true; color: "#1e1e2f" }

                Text { id: densityText; text: "Локальная плотность пыли: 0.0%"; color: "white"; font.bold: true; font.pixelSize: 12 }

                Rectangle {
                    id: alarmBanner
                    Layout.fillWidth: true; height: 50; radius: 6; color: "#313244"
                    Behavior on color { ColorAnimation { duration: 200 } }
                    Text { id: alarmLabel; anchors.centerIn: parent; text: "ОЖИДАНИЕ"; color: "white"; font.bold: true; font.pixelSize: 16 }
                }

                Rectangle { height: 1; Layout.fillWidth: true; color: "#1e1e2f" }

                Text { text: "ЖУРНАЛ СИСТЕМЫ"; color: "white"; font.bold: true; font.pixelSize: 11 }

                Rectangle {
                    Layout.fillWidth: true; Layout.fillHeight: true; color: "#0c0c0e"; border.color: "#1e1e2f"; border.width: 1; radius: 6
                    ScrollView {
                        anchors.fill: parent; anchors.margins: 8; clip: true
                        TextArea { id: logTextArea; readOnly: true; color: "#a6adc8"; font.family: "Monospace"; font.pixelSize: 10; text: ""; wrapMode: TextArea.Wrap; background: null }
                    }
                }
            }
        }
    }
}
