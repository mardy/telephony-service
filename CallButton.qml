import QtQuick 1.1

IconButton {
    width: 116
    height: 37
    icon: "assets/call_icon.png"
    borderWidth: 3
    borderColor: "#00be4d"
    color: "#3ac974"
    onClicked: print("calling!")
}