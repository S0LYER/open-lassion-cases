import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: sliderRoot
    implicitWidth: 200
    implicitHeight: 24
    Layout.fillWidth: true

    // Slider control
    property int totalFrames: 1
    property int currentFrame: 0
    property string playState: "inactive"
    
    property real wavePhase: 0.0

    // Wave animation
    NumberAnimation {
        target: sliderRoot
        property: "wavePhase"
        from: 0
        to: Math.PI * 2
        duration: 1000
        loops: Animation.Infinite
        running: sliderRoot.playState === "playing"
        
        onRunningChanged: {
            if (!running) {
                sliderRoot.wavePhase = 0.0; 
            }
        }
    }

    onWavePhaseChanged: canvas.requestPaint()
    onCurrentFrameChanged: canvas.requestPaint()

    Slider {
        id: internalSlider
        anchors.fill: parent
        from: 0
        to: sliderRoot.totalFrames > 1 ? sliderRoot.totalFrames - 1 : 1
        value: sliderRoot.currentFrame
        live: true 

        background: Item {
            anchors.fill: parent
            
            Canvas {
                id: canvas
                anchors.fill: parent
                antialiasing: true

                onPaint: {
                    var ctx = getContext("2d");
                    ctx.reset();
                    ctx.clearRect(0, 0, width, height);

                    var centerY = height / 2;
                    var activeWidth = (internalSlider.visualPosition * (width - internalSlider.handle.width)) + (internalSlider.handle.width / 2);
                    var totalWidth = width;

                    var isPlaying = (sliderRoot.playState === "playing");

                    ctx.beginPath();
                    ctx.lineWidth = 2;
                    ctx.strokeStyle = "#ff8c00";
                    
                    ctx.moveTo(0, centerY);
                    if (isPlaying) {
                        var freq = 0.12;
                        var amp = 4.0;   // Wave height
                        for (var x = 0; x <= activeWidth; x++) {
                            // Sinusoid
                            var y = centerY + Math.sin(x * freq - sliderRoot.wavePhase * 5) * amp;
                            ctx.lineTo(x, y);
                        }
                    } else {
                        ctx.lineTo(activeWidth, centerY);
                    }
                    ctx.stroke();

                    ctx.beginPath();
                    ctx.lineWidth = 2;
                    ctx.strokeStyle = "#313244"; 
                    ctx.moveTo(activeWidth, centerY);
                    ctx.lineTo(totalWidth, centerY);
                    ctx.stroke();
                }
            }
        }

        handle: Rectangle {
            x: internalSlider.leftPadding + internalSlider.visualPosition * (internalSlider.availableWidth - width)
            y: internalSlider.topPadding + internalSlider.availableHeight / 2 - height / 2
            implicitWidth: 16
            implicitHeight: 16
            radius: 8
            color: "white"
            border.color: "#ff8c00"
            border.width: 1
            scale: internalSlider.hovered || internalSlider.pressed ? 1.2 : 1.0
            Behavior on scale { NumberAnimation { duration: 100 } }
        }

        onMoved: {
            videoBackend.setVideoPosition(Math.round(value))
        }
    }
}
