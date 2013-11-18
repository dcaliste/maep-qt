import QtQuick 2.0
import Sailfish.Silica 1.0
import Qt.labs.folderlistmodel 1.0

Dialog {
id: trackopen_dialog
property string title
property string selection
}

/*	Dialog {
	    id: trackopen_dialog
	    property string title
	    property alias folder: folderModel.folder
	    property string selection

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
			return ""
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
		id: trackopen_header
	        Column {
		    width: parent ? parent.width : undefined
	            DialogHeader {
		        title: trackopen_dialog.title
	            }
	            Row {
		        id: trackopen_head
		        height: Theme.itemSizeSmall
		        width: parent.width
		        IconButton {
		            id: trackopen_back
		            icon.source: "image://theme/icon-header-back"
		            enabled: folderModel.dirname().length > 0
		            onClicked: { folderModel.navigateUp() }
		        }
		        Button {
		            width: parent.width - trackopen_back.width - trackopen_options.width
		            text: folderModel.basename()
		            visible: folderModel.dirname().length > 0
		            enabled: false
		        }
		        IconButton {
		            id: trackopen_options
		            icon.source: "image://theme/icon-m-levels"
		            onClicked: { trackopen_controls.open = !trackopen_controls.open }
		        }
	            }
	        }
	    }
	    SilicaListView {
		id: trackopen_list
		header: trackopen_header
		anchors {
		    fill: parent
		    rightMargin: page.isPortrait ? 0 : trackopen_controls.visibleSize
                    bottomMargin: page.isPortrait ? trackopen_controls.visibleSize + Theme.paddingLarge: 0
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
		    contentHeight: Theme.itemSizeSmall * 0.75
		    Image {
			id: trackopen_icon
			source: "image://theme/icon-m-folder"
			visible: fileIsDir
		    }
		    Label {
                        text: fileName
		        font.pixelSize: Theme.fontSizeSmall
		        anchors.fill: parent
			anchors.leftMargin: trackopen_icon.width + Theme.paddingSmall
		        color: highlighted ? Theme.highlightColor : Theme.primaryColor
		    }
		    Label {
		        font.pixelSize: Theme.fontSizeExtraSmall
		        text: formatter.formatDate(fileModified, Formatter.Timepoint) + " - " + formatter.formatFileSize(fileSize)
		        color: Theme.secondaryColor
		        anchors.right: parent.right
		        anchors.bottom: parent.bottom
		    }
	            onClicked: { fileIsDir ? folderModel.folder = filePath : selection = filePath }
		}
	    }
	    VerticalScrollDecorator { flickable: trackopen_list }
	    DockedPanel {
        	id: trackopen_controls
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
	}*/
