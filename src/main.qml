import QtQuick 2.0
import Sailfish.Silica 1.0
import Maep 1.0
import QtWebKit 3.0
import QtPositioning 5.0
//import QtQuick.Dialogs 1.0
//import "pages"

ApplicationWindow
{
    initialPage: Page {
    id: page

    // To enable PullDownMenu, place our content in a SilicaFlickable
    SilicaFlickable {
        id: header
	z: +1
        anchors.top: parent.top

        // PullDownMenu and PushUpMenu must be declared in SilicaFlickable, SilicaListView or SilicaGridView
        PullDownMenu {
            /*MenuItem {
                text: "Show Page 2"
                //onClicked: pageStack.push(Qt.resolvedUrl("SecondPage.qml"))
            }*/
	    MenuItem {
		TextSwitch {
		    id: wikicheck
		    anchors.horizontalCenter: parent.horizontalCentre
		    text: "Wikipedia"
		    checked: map.wiki_status
                    onCheckedChanged: {
                        map.setWikiStatus(checked)
                    }
		}
                onClicked: {wikicheck.checked = !wikicheck.checked}
	    }
        }
	onMovementStarted: { map.opacity = 0.25 }
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
		    label = "Place search"
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
                width: 120
                title: "Mæp"
            }
        }
    }
    GpsMap {
        id: map
        anchors.top: header.bottom
        width: page.width
	z: -1
        height: page.height - header.height
        onSearchRequest: { search.focus = true }
        onWikiURLSelected: { pageStack.push(wiki) }
	onWikiStatusChanged: { wikicheck.checked = status }
        onSearchResults: { placeview.model = map.search_results; placeview.visible = true; placeview.focus = true;
                           busy.running = false; search.label = search_results.length + " place(s) found" }
    }
    /*ListModel {
        id: placemodel
        model: map.getSearchResults()
    }*/
    SilicaListView {
        id: placeview
        z: 0
        width: parent.width * 0.75
	height: Math.min(childrenRect.height, page.height - header.height)
	contentHeight: childrenRect.height
	anchors.top: header.bottom
	anchors.horizontalCenter: parent.horizontalCenter
        visible: false

        delegate: ListItem {
            width: parent.width
            height: Theme.itemSizeSmall

            Rectangle {
		width: parent.width
		height: parent.height
		color: "black"
		opacity: 0.85
		Label {
                    text: model.name + ", " + model.country
		    anchors.fill: parent
		    anchors.verticalCenter: parent.verticalCenter
		    color: highlighted ? Theme.highlightColor : Theme.primaryColor
		}
            }
	    onClicked: { placeview.visible = false; map.setLookAt(model.latitude, model.longitude) }
	    onFocusChanged: { visible = focus }
        }
	VerticalScrollDecorator { flickable: placeview }
	onFocusChanged: { if (!focus) { visble = false } }
    }    
    }
    //cover: Qt.resolvedUrl("cover/CoverPage.qml")    

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
