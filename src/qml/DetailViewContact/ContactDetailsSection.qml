/*
 * Copyright 2012-2013 Canonical Ltd.
 *
 * This file is part of phone-app.
 *
 * phone-app is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * phone-app is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

import QtQuick 2.0
import Ubuntu.PhoneApp 0.1
import "../Widgets" as LocalWidgets
import Ubuntu.Components 0.1
import Ubuntu.Components.ListItems 0.1 as ListItem
import "DetailTypeUtilities.js" as DetailUtils

FocusScope {
    id: contactDetailsSection

    property alias model: details.model
    property alias delegate: details.delegate

    property variant detailTypeInfo
    property bool editable
    onEditableChanged: if (!editable) newItems.model.clear()

    signal detailAdded()
    signal scrollRequested(real y, real height)

    height: col.height

    function save() {
        var added = [];
        for (var i = 0; i < col.children.length; i++) {
            var child = col.children[i];
            if (child && child.item && child.item.save && child.item.save instanceof Function &&
               !child.item.deleted) {
                var valid = child.item.save();
                if (child.item.added && valid) {
                    added.push(child.item.detail);
                }
            }
        }
        return added;
    }

    function appendNewItem() {
        newItems.model.append({})
        detailAdded()
    }

    Column {
        id: col
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top

        Repeater {
            id: details
            // model and delegate are set via property alias
        }

        Repeater {
            id: newItems
            model: ListModel {}
            delegate: Loader {
                id: newItem
                source: detailTypeInfo.delegateSource
                anchors.left: (parent) ? parent.left : undefined
                anchors.right: (parent) ? parent.right : undefined

                Binding { target: item; property: "detailTypeInfo"; value: contactDetailsSection.detailTypeInfo }
                Binding { target: item; property: "editable"; value: contactDetailsSection.editable }
                Binding { target: item; property: "added"; value: true }

                opacity: editable ? 1.0 : 0.0

                onLoaded: {
                    item.detail = Qt.createQmlObject("import Ubuntu.PhoneApp 0.1; " + detailTypeInfo.newItemType + "{}", newItem);
                    if (detailTypeInfo.subTypes.length > 0) DetailUtils.setDetailSubType(item.detail, detailTypeInfo.subTypes[0]);
                    item.focus = true;
                    item.focusRequested();
                }

                Connections {
                    target: item
                    onScrollRequested: {
                        var position = contactDetailsSection.mapFromItem(item, item.x, item.y)
                        contactDetailsSection.scrollRequested(position.y, item.height)
                    }
                }
            }
            onItemAdded: item.focus = true
        }

        Item {
            anchors.left: parent.left
            anchors.right: parent.right
            height: editable ? units.gu(4) : 0
            opacity: editable ? 1.0 : 0.0
            Behavior on height { LocalWidgets.StandardAnimation {}}
            Behavior on opacity { LocalWidgets.StandardAnimation {}}

            Label {
                id: addText

                anchors.left: parent.left
                anchors.leftMargin: units.gu(2)
                anchors.verticalCenter: parent.verticalCenter
                anchors.verticalCenterOffset: units.dp(-1)
                //fontSize: "x-small"
                elide: Text.ElideRight
                color: Qt.rgba(0.4, 0.4, 0.4, 1.0)
                text: (detailTypeInfo.newItemText) ? detailTypeInfo.newItemText : ""
            }

            MouseArea {
                anchors.fill: parent
                onClicked: appendNewItem()
            }

            ListItem.ThinDivider {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
            }
        }
    }
}