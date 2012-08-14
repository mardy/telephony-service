import QtQuick 1.1
import "../Widgets"

// FIXME: the ListView could be the toplevel item
Item {
    id: messageList

    property alias model: listView.model
    property string filter

    ListView {
        id: listView
        // FIXME: after moving the ListView to the toplevel, remove this anchor
        anchors.fill: parent
        clip: true
        delegate: MessageDelegate {
            id: messageDelegate

            anchors.left: parent.left
            anchors.right: parent.right
            onClicked: telephony.startChat(customId, phoneNumber, threadId)
            selected: telephony.messages.loaded
                      && !telephony.view.newMessage
                      && (threadId != "" && (telephony.view.threadId == threadId)
                      || contactModel.comparePhoneNumbers(telephony.view.number, phoneNumber))
        }
    }

    ScrollbarForListView {
        view: listView
    }
}
