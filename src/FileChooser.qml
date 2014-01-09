/*
 * FileChooser.qml
 * Copyright (C) Damien Caliste 2013 <dcaliste@free.fr>
 *
 * FileChooser.qml is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

import QtQuick 2.0
import Sailfish.Silica 1.0
import Qt.labs.folderlistmodel 1.0

Item {
    id: chooser_item
    property Component title: chooser_title
    property alias folder: folderModel.folder
    property string selection
    property string entry
    property bool saveMode: false

    FolderListModel {
	id: folderModel
        folder: StandardPaths.documents
	function basename() {
            var url = folder.toString()
	    if (url == "file:///home/nemo") {
		return "My Jolla"
	    } else {
                return url.substring(url.lastIndexOf("/") + 1)
	    }
        }
	function dirname() {
	    var url = parentFolder.toString()
	    if (url == "file:///home") {
		return "file:///media"
	    } else if (url == "file:///") {
		return "file:///home/nemo"
	    } else {
		return url
	    }
	}
	function navigateUp() {
	    var url = dirname()
	    folder = url
	    if (url == "file:///media") {
		append({"fileName": "My Jolla", "filePath": "file:///home/nemo"})
	    }
	}
    }
    Component {
	id: chooser_title
	Label { text: "Select a file" }
    }
    Component {
	id: chooser_header
	Column {
	    width: parent ? parent.width: undefined
	    Loader {
                sourceComponent: chooser_item.title
                anchors.right: parent.right
                width: parent.width
            }
	    Row {
		width: parent.width
	        TextField {
		    id: chooser_entry
		    width: parent.width - chooser_adddir.width
		    placeholderText: "enter a new file name"
		    validator: RegExpValidator { regExp: /^[^/]+$/ }
		    inputMethodHints: Qt.ImhNoPredictiveText
		    EnterKey.text: "save"
		    EnterKey.onClicked: { if (acceptableInput) { chooser_item.entry = folderModel.folder + "/" + text } }
	        }
		IconButton {
		    id: chooser_adddir
		    icon.source: "image://theme/icon-m-folder"
		    enabled: chooser_entry.acceptableInput
		    onClicked: {  }
		}
		visible: chooser_item.saveMode
	    }
	    Label {
		width: parent.width
		visible: chooser_item.saveMode
		text: "or select an existing one"
		font.pixelSize: Theme.fontSizeSmall
		color: Theme.secondaryColor
		horizontalAlignment: Text.AlignHCenter
	    }
	    Row {
		id: chooser_head
		height: Theme.itemSizeSmall
		width: parent.width
		IconButton {
		    id: chooser_back
		    icon.source: "image://theme/icon-header-back"
		    enabled: folderModel.dirname().length > 0
		    onClicked: { folderModel.navigateUp() }
		}
		Button {
		    width: parent.width - chooser_back.width - chooser_options.width
		    text: folderModel.basename()
		    visible: folderModel.dirname().length > 0
		    enabled: false
		}
		IconButton {
		    id: chooser_options
		    icon.source: "image://theme/icon-m-levels"
		    onClicked: { chooser_controls.open = !chooser_controls.open }
		}
	    }
	}
    }
    SilicaListView {
	id: chooser_list
	header: chooser_header
	anchors {
	    fill: parent
	    rightMargin: page.isPortrait ? 0 : chooser_controls.visibleSize
            bottomMargin: page.isPortrait ? chooser_controls.visibleSize + Theme.paddingLarge: 0
	}
	model: folderModel
	Formatter {
            id: formatter
        }
	ViewPlaceholder {
            enabled: folderModel.count == 0
            text: "No files"
        }
	delegate: ListItem {
	    contentHeight: Theme.itemSizeSmall
	    Image {
		id: chooser_icon
		source: fileIsDir ? "image://theme/icon-m-folder" : "image://theme/icon-m-document"
		//visible: fileIsDir
                anchors.left: parent.left
                anchors.leftMargin: Theme.paddingSmall
                anchors.rightMargin: Theme.paddingSmall
                anchors.verticalCenter: parent.verticalCenter
	    }
	    Label {
                text: fileName
                truncationMode: TruncationMode.Fade
		font.pixelSize: Theme.fontSizeSmall
                anchors.topMargin: Theme.paddingSmall
                anchors.right: parent.right
		anchors.left: chooser_icon.right
		color: highlighted ? Theme.highlightColor : Theme.primaryColor
	    }
	    Label {
		font.pixelSize: Theme.fontSizeExtraSmall
		text: formatter.formatDate(fileModified, Formatter.Timepoint) + " - " + formatter.formatFileSize(fileSize)
		color: Theme.secondaryColor
		anchors.right: img_go.left
		anchors.bottom: parent.bottom
	    }
            Image {
                id: img_go
                source: "image://theme/icon-m-right"
                anchors.right: parent.right
                anchors.leftMargin: Theme.paddingSmall
                anchors.rightMargin: Theme.paddingSmall
                anchors.verticalCenter: parent.verticalCenter
            }
	    onClicked: { fileIsDir ? folderModel.folder = filePath : selection = filePath }
	}
    }
    VerticalScrollDecorator { flickable: chooser_list }
    DockedPanel {
        id: chooser_controls
	open: false

        width: page.isPortrait ? parent.width : Theme.itemSizeSmall
        height: page.isPortrait ? Theme.itemSizeSmall : parent.height

        dock: page.isPortrait ? Dock.Bottom : Dock.Right
	
	Flow {
	    anchors.fill: parent

            anchors.centerIn: parent
	    ComboBox {
		width: page.isPortrait ? parent.width / 2 : parent.width
		label: "sort by"
		menu: ContextMenu {
        	    MenuItem { text: "name"; onClicked: { folderModel.sortField = FolderListModel.Name } }
        	    MenuItem { text: "date"; onClicked: { folderModel.sortField = FolderListModel.Time } }
		    MenuItem { text: "type"; onClicked: { folderModel.sortField = FolderListModel.Type } }
		}
	    }
	    TextSwitch {
		width: page.isPortrait ? parent.width / 2 : parent.width
		text: "reversed"
		onCheckedChanged: { folderModel.sortReversed = checked }
	    }
	}
    }
}
