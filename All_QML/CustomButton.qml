import QtQuick
import QtQuick.Controls
import QtQuick.Layouts 

Button {
    id: controlBtn
    
    // Text/buttons settings
    property color baseColor: "#000000"
    property color hoverColor: "#ffffff"
    property color textColor: "#ffffff"
    property color textHoverColor: "#000000"
    property int buttonRadius: 6
    property int textSize: 11
    property bool buttonBold: true

    Layout.fillWidth: true
    Layout.preferredHeight: 38
    antialiasing: true

    background: Rectangle {
        color: controlBtn.hovered ? controlBtn.hoverColor : controlBtn.baseColor
        border.color: "white"
        border.width: 1
        radius: controlBtn.buttonRadius
        antialiasing: true
        
        // Liquid color 150ms
        Behavior on color { ColorAnimation { duration: 150 } }
    }

    contentItem: Text {
        text: controlBtn.text
        color: controlBtn.hovered ? controlBtn.textHoverColor : controlBtn.textColor
        font.bold: controlBtn.buttonBold
        font.pixelSize: controlBtn.textSize
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        antialiasing: true
    }
}
