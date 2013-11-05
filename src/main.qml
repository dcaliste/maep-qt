import QtQuick 2.0
import Sailfish.Silica 1.0
import Maep 1.0
import QtWebKit 3.0
//import "pages"

ApplicationWindow
{
    initialPage: page
    cover: coverPage

    Page {
    id: page

    // To enable PullDownMenu, place our content in a SilicaFlickable
    SilicaFlickable {
        id: header
        anchors.top: parent.top

        // PullDownMenu and PushUpMenu must be declared in SilicaFlickable, SilicaListView or SilicaGridView
        PullDownMenu {
            MenuItem {
                text: "About Mæp"
                //onClicked: pageStack.push(aboutpage)
            }
	    MenuItem {
		TextSwitch {
		    id: wikicheck
		    anchors.horizontalCenter: parent.horizontalCenter
		    text: "Wikipedia"
		    checked: map.wiki_status
                    onCheckedChanged: {
                        map.setWikiStatus(checked)
                    }
		}
                onClicked: {wikicheck.checked = !wikicheck.checked}
	    }
            MenuItem {
                TextSwitch {
                    id: trackcheck
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "Track capture"
                    checked: map.track_capture
                    onCheckedChanged: {
                        map.setTrackCapture(checked)
                    }
                }
                onClicked: {trackcheck.checked = !trackcheck.checked}
            }
            MenuItem {
                text: "Import track"
                font.pixelSize: Theme.fontSizeSmall
		color: Theme.secondaryColor
                //onClicked: pageStack.push(Qt.resolvedUrl("SecondPage.qml"))
            }
            MenuItem {
                text: "Export track"
                visible: map.track_available
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.secondaryColor
                //onClicked: pageStack.push(Qt.resolvedUrl("SecondPage.qml"))
            }
            MenuItem {
                text: "Clear track"
                visible: map.track_available
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.secondaryColor
                onClicked: map.clearTrack()
            }
        }
	onMovementStarted: { map.opacity = Theme.highlightBackgroundOpacity }
        onMovementEnded: { map.opacity = 1 }

        // Tell SilicaFlickable the height of its content.
        width: page.width
	height: 100 //childrenRect.height
        contentHeight: childrenRect.height

        // Place our content in a Column.  The PageHeader is always placed at the top
        // of the page, followed by our content.
        Row {
            width: parent.width
	    spacing: 0 /*Theme.paddingSmall*/
            TextField {
                id: search
                width: parent.width - maep.width
                placeholderText: "Enter a place name"
	        label: "Place search"
		anchors.verticalCenter: parent.verticalCenter
		EnterKey.onClicked: {
		    busy.running = true
		    map.focus = true
		    placeview.model = null
                    map.setSearchRequest(text)
		}
		onFocusChanged: { if (focus) { selectAll() } }
            }
	    BusyIndicator {
	   	id: busy
                running: false
                visible: true
                size: BusyIndicatorSize.Small
                anchors.verticalCenter: parent.verticalCenter
            }
            PageHeader {
                id: maep
                width: 130
                title: "Mæp"
            }
        }
    }
    Drawer {
 	id: drawer
        anchors.top: header.bottom
        width: page.width
        height: page.height - header.height

        dock: page.isPortrait ? Dock.Top : Dock.Left

        background: placeview

    GpsMap {
        id: map
        /*anchors.top: header.bottom
        width: page.width
        height: page.height - header.height*/
	anchors.fill: parent
        onSearchRequest: { search.focus = true; search.label = "Place search" }
        onWikiURLSelected: { pageStack.push(wiki) }
	onWikiStatusChanged: { wikicheck.checked = status }
        onSearchResults: { busy.running = false; search.label = search_results.length + " place(s) found";
			   /*pageStack.push(placepage)*/
			   drawer.open = true
			   placeview.model = search_results }
	Behavior on opacity {
            FadeAnimation {}
        }
    }
    }

    Dialog {
	id: placepage
	property string place

    SilicaListView {
        id: placeview
        anchors.fill: parent
        /*header: DialogHeader {
            title: "Select a place"
        }*/

	PullDownMenu {
	    MenuItem {
		text: "Cancel"
		onClicked: { drawer.open = false }
	    }
	}

        delegate: ListItem {
		Label {
                    text: model.name + ", " + model.country
		    //label: "some minor info"
		    height: Theme.itemSizeSmall
		    font.pixelSize: Theme.fontSizeSmall
		    anchors.fill: parent
		    color: highlighted ? Theme.highlightColor : Theme.primaryColor
		}
	    onClicked: { search.text = model.name
	    		 drawer.open = false
			 /*placepage.place = model.name
	                 placepage.accept()*/
			 map.setLookAt(model.latitude, model.longitude) }
        }
	VerticalScrollDecorator { flickable: placeview }
    }}
    }

    CoverBackground {
	id: coverPage
	property bool active: status == Cover.Active
	/*CoverPlaceholder {*/
            GpsMapCover {
                width: parent.width
                height: parent.height
                map: map
	        status: coverPage.active
            }
        /*}*/
    }
    CoverActionList {
        enabled: true
        iconBackground: true
        CoverAction {
            iconSource: "image://theme/icon-m-remove"
            onTriggered: map.zoomOut()
        }
        CoverAction {
            iconSource: "image://theme/icon-m-add"
            onTriggered: map.zoomIn()
        }
    }

    Component {
        id: wiki

        Page {
            PageHeader {
                id: wikititle
                title: map.wiki_title
            }
	    Item {
		id: thumbnail
		visible: (map.wiki_thumbnail != "")
		width: wikiimg.width + Theme.paddingSmall * 2
		height: wikiimg.height + Theme.paddingSmall * 2
		anchors.top: wikititle.bottom
                anchors.topMargin: Theme.paddingLarge
                anchors.horizontalCenter: wikititle.horizontalCenter
		Rectangle {
		    id: frame
		    color: Theme.secondaryColor
		    radius: Theme.paddingSmall
		    anchors.fill: parent
		    opacity: 0.5
		}
		Image {
		    id: wikiimg
		    anchors.centerIn: frame
       		    source: map.wiki_thumbnail
       		    sourceSize.width: 360
       		    sourceSize.height: 360
    		}
            }
	    Label {
		id: coordinates
		anchors.top: thumbnail.bottom
		anchors.right: parent.right
		anchors.rightMargin: Theme.paddingMedium
		text: "coordinates: " + map.getWikiPosition()
		color: Theme.secondaryColor
		font.pixelSize: Theme.fontSizeExtraSmall
	    }
	    Label {
                id: body
                text: map.wiki_summary
                font.pixelSize: Theme.fontSizeSmall
                wrapMode: Text.WordWrap
		width: parent.width - Theme.paddingMedium * 2
		horizontalAlignment: Text.AlignJustify
		anchors.topMargin: Theme.paddingLarge
		anchors.horizontalCenter: wikititle.horizontalCenter
                anchors.top: coordinates.bottom
	    }
	    Button {
		text: "View Wikipedia page"
		onClicked: pageStack.push(wikipedia)
		anchors.top: body.bottom
		anchors.topMargin: Theme.paddingLarge
		anchors.horizontalCenter: wikititle.horizontalCenter
	    }
	    Label {
		anchors.bottom: parent.bottom
		anchors.right: parent.right
		anchors.rightMargin: Theme.paddingMedium
		font.pixelSize: Theme.fontSizeExtraSmall
		font.italic: true
		color: Theme.secondaryColor
		text: "source: geonames.org"
	    }
        }
    }
    Component {
        id: wikipedia
        
        Page {
            PageHeader {
                id: wikititle
                title: map.wiki_title
            }
           WebView {
                anchors.top: wikititle.bottom
                width: page.width
                height: page.height - wikititle.height
                url: map.wiki_url
            }
	   /*FileDialog {
    id: fileDialog
    title: "Please choose a file"
    onAccepted: {
        console.log("You chose: " + fileDialog.fileUrls)
        Qt.quit()
    }
    onRejected: {
        console.log("Canceled")
        Qt.quit()
    }
    Component.onCompleted: visible = true
}*/
        }
    }
}
