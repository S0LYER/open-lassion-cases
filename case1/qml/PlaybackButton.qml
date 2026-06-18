import QtQuick
import QtQuick.Controls
import QtQuick.Layouts 

Button {
    id: playBtn
    
    property string stateValue: "inactive"

    // ⏸ ▶ ↻ 
    text: stateValue === "playing" ? "⏸" : (stateValue === "ended" ? "↻" : "▶")
    Layout.preferredWidth: 48
    Layout.preferredHeight: 48
    antialiasing: true

    background: Rectangle {
        color: playBtn.hovered ? "black" : "white"
        border.color: "white" 
        border.width: 1
        radius: 24
        antialiasing: true
        Behavior on color { ColorAnimation { duration: 150 } }
    }

    contentItem: Text {
        text: playBtn.text
        color: playBtn.hovered ? "white" : "black"
        font.pixelSize: 18
        font.bold: true
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        
        leftPadding: playBtn.text === "▶" ? 4 : 0
        topPadding: playBtn.text === "▶" ? 1 : 0
    }
}
