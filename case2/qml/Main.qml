import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

ApplicationWindow {
    id: rootWindow
    visible: true
    width: 1024
    height: 720
    title: "Пассажиропоток (YOLOv4-tiny)"
    color: "#000000" // AMOLED 

    property string playState: "inactive" // "inactive", "playing", "paused", "ended"
    property int frameCounter: 0

    property int totalFrames: 1
    property int currentFrame: 0

    property int countIn: 0
    property int countOut: 0
    property int countInside: 0

    footer: Rectangle {
        height: 30; color: "#000000"; border.color: "#1e1e2f"; border.width: 1
        Text {
            id: statusBarText; text: "Готов"
            color: "#ffffff"; font.pixelSize: 11; anchors.verticalCenter: parent.verticalCenter; anchors.left: parent.left; anchors.leftMargin: 10
        }
    }

    Connections {
        target: videoBackend
        
        function onVideoEnded() { 
            playState = "ended" 
        }
        
        function onStatusMessage(msg) { 
            statusBarText.text = msg 
        }
        
        function onFrameReady(frame) {
            frameCounter++
            videoImage.source = "image://video/frame_" + frameCounter
        }
        
        function onStatsUpdated(inC, outC, insideC) {
            countIn = inC
            countOut = outC
            countInside = insideC
        }
        
        function onVideoOpened(total) { 
            totalFrames = total 
        }
        
        function onFramePositionChanged(current) { 
            currentFrame = current 
        }
    }

    FileDialog {
        id: cfgDialog; title: "Выберите конфигурацию (.cfg)"
        nameFilters: ["Config (*.cfg)"]
        onAccepted: videoBackend.loadModel(cfgDialog.currentFile)
    }

    FileDialog {
        id: weightsDialog; title: "Выберите веса модели (.weights)"
        nameFilters: ["Weights (*.weights)"]
        onAccepted: videoBackend.loadWeights(weightsDialog.currentFile)
    }

    FileDialog {
        id: videoDialog; title: "Выберите видеопоток"
        nameFilters: ["Видео файлы (*.mp4 *.avi *.mkv)"]
        onAccepted: { frameCounter = 0; playState = "playing"; videoBackend.loadVideo(videoDialog.currentFile) }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true; Layout.fillHeight: true; color: "#000000"

            Image {
                id: videoImage; anchors.fill: parent; anchors.margins: 10
                fillMode: Image.PreserveAspectFit; source: "image://video/frame_0"; cache: false
            }

            Rectangle {
                anchors.top: parent.top; anchors.horizontalCenter: parent.horizontalCenter; anchors.margins: 25
                width: overlayText.width + 45; height: 48
                color: "#cc000000"; border.color: "white"; border.width: 1; radius: 10

                Text {
                    id: overlayText; anchors.centerIn: parent
                    text: "ВОШЛО: " + countIn + "  |  ВЫШЛО: " + countOut + "  |  ВНУТРИ: " + countInside
                    color: "white"; font.pixelSize: 15; font.bold: true
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

                    GridLayout {
                        columns: 3; rowSpacing: 8; columnSpacing: 8

                        CustomButton { text: "1. Модель (.cfg)"; Layout.preferredWidth: 150; onClicked: cfgDialog.open() }
                        CustomButton { text: "2. Веса (.weights)"; Layout.preferredWidth: 150; onClicked: weightsDialog.open() }
                        CustomButton { text: "3. Видео (.mp4)"; Layout.preferredWidth: 150; onClicked: videoDialog.open() }
                        CustomButton { text: "Остановить"; Layout.preferredWidth: 150; visible: playState === "playing" || playState === "paused"; onClicked: { videoBackend.stopVideo(); playState = "inactive" } }
                        CustomButton { text: "Сбросить статистику"; Layout.preferredWidth: 150; onClicked: videoBackend.resetStats() }
                    }

                        spacing: 4
                        Text { text: "РЕЖИМ ПОДСЧЕТА:"; color: "white"; font.bold: true; font.pixelSize: 11 }
                        RowLayout {
                            spacing: 12
                            ButtonGroup { id: modeGroup }
                            
                            RadioButton {
                                id: rbAuto
                                text: "Авто (Вход/Выход)"
                                checked: true
                                ButtonGroup.group: modeGroup
                                contentItem: Text {
                                    text: parent.text
                                    color: "white"
                                    font.pixelSize: 11
                                    leftPadding: parent.indicator.width + parent.spacing
                                }
                                onCheckedChanged: {
                                    if (checked) videoBackend.setMode(0)
                                }
                            }

                            RadioButton {
                                id: rbIn
                                text: "Только ВХОД"
                                ButtonGroup.group: modeGroup
                                contentItem: Text {
                                    text: parent.text
                                    color: "white"
                                    font.pixelSize: 11
                                    leftPadding: parent.indicator.width + parent.spacing
                                }
                                onCheckedChanged: {
                                    if (checked) videoBackend.setMode(1)
                                }
                            }

                            RadioButton {
                                id: rbOut
                                text: "Только ВЫХОД"
                                ButtonGroup.group: modeGroup
                                contentItem: Text {
                                    text: parent.text
                                    color: "white"
                                    font.pixelSize: 11
                                    leftPadding: parent.indicator.width + parent.spacing
                                }
                                onCheckedChanged: {
                                    if (checked) videoBackend.setMode(2)
                                }
                            }
                        }
                    }

                    Item { Layout.fillWidth: true }

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

                    Item { Layout.fillWidth: true }
                }
            }
        }
    }
}
