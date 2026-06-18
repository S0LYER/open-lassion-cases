import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

ApplicationWindow {
    id: rootWindow
    visible: true
    width: 1200
    height: 720
    title: "Классификатор дисперсности грузов"
    color: "#000000" // AMOLED

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
        function onVideoEnded() { playState = "ended" }
        function onStatusMessage(msg) { statusBarText.text = msg }
        function onLogMessage(msg) { logTextArea.append(msg) }
        function onFrameReady(frame) {
            frameCounter++
            videoImage.source = "image://video/frame_" + frameCounter
        }
        function onVideoOpened(total) { totalFrames = total }
        function onFramePositionChanged(current) { currentFrame = current }
    }

    FileDialog {
        id: fileDialog
        title: "Выберите видео загрузки груза"
        nameFilters: ["Видео файлы (*.mp4 *.avi *.mkv)"]
        onAccepted: {
            logTextArea.text = "" 
            frameCounter = 0
            playState = "playing"
            videoBackend.loadVideo(fileDialog.currentFile)
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredWidth: 3 
            spacing: 0

            Rectangle {
                Layout.fillWidth: true; Layout.fillHeight: true; color: "#000000"

                Image {
                    id: videoImage
                    anchors.fill: parent; anchors.margins: 10
                    fillMode: Image.PreserveAspectFit
                    source: "image://video/frame_0"
                    cache: false
                }
            }

            Rectangle {
                Layout.fillWidth: true; height: 70; color: "#000000"; border.color: "#1e1e2f"; border.width: 1

                RowLayout {
                    anchors.fill: parent; anchors.margins: 15; spacing: 20

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

                    Text { text: "Кадр"; color: "white"; font.bold: true; font.pixelSize: 11; visible: playState !== "inactive" }
                    WavySlider { id: videoSeeker; totalFrames: rootWindow.totalFrames; currentFrame: rootWindow.currentFrame; playState: rootWindow.playState; visible: playState !== "inactive" }
                }
            }
        }

        Rectangle { width: 1; Layout.fillHeight: true; color: "#1e1e2f" }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredWidth: 1 
            color: "#000000"

            ColumnLayout {
                anchors.fill: parent; anchors.margins: 15; spacing: 15

                CustomButton {
                    text: "Подгрузить видео с ПК"
                    baseColor: "#89b4fa"      
                    hoverColor: "#74c7ec"     
                    textColor: "#11111b"      
                    textHoverColor: "#11111b"
                    buttonBold: true
                    textSize: 14
                    Layout.preferredHeight: 50
                    onClicked: fileDialog.open()
                }

                CustomButton {
                    text: "Остановить"
                    Layout.preferredHeight: 40
                    visible: playState === "playing" || playState === "paused"
                    onClicked: { videoBackend.stopVideo(); playState = "inactive" }
                }

                Rectangle { height: 1; Layout.fillWidth: true; color: "#1e1e2f" }

                Text { text: "ДЕТАЛИЗИРОВАННЫЙ ЛОГ:"; color: "white"; font.bold: true; font.pixelSize: 12 }

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
                            id: logTextArea
                            readOnly: true
                            color: "#a6adc8"
                            font.family: "Monospace"
                            font.pixelSize: 11
                            text: "Ожидание загрузки видео..."
                            wrapMode: TextArea.Wrap
                            background: null
                        }
                    }
                }
            }
        }
    }
}
