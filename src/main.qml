import QtQuick 2.0
import Sailfish.Silica 1.0
import Maep 1.0
import QtWebKit 3.0
import QtPositioning 5.0
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
		EnterKey.text: "search"
		EnterKey.onClicked: {
		    busy.running = true
		    map.focus = true
		    drawer.open = false
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
        onWikiEntryChanged: { pageStack.push(wiki) }
	onWikiStatusChanged: { wikicheck.checked = status }
        onSearchResults: { search.label = search_results.length + " place(s) found"
			   busy.running = false
			   drawer.open = true
			   placeview.model = search_results
			 }
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
		text: "Cancel search"
		onClicked: { drawer.open = false }
	    }
	}
	PushUpMenu {
            MenuItem {
                text: "Cancel search"
                onClicked: { drawer.open = false }
            }
        }

	ViewPlaceholder {
            enabled: placeview.count == 0
            text: "No result"
        }

        delegate: ListItem {
		
		contentHeight: Theme.itemSizeSmall * 0.75
		Label {
                    text: model.name + ", " + model.country
		    font.pixelSize: Theme.fontSizeSmall
		    anchors.fill: parent
		    color: highlighted ? Theme.highlightColor : Theme.primaryColor
		}
		Label {
		    property real dist: map.coordinate.distanceTo(model.coordinate)
		    font.pixelSize: Theme.fontSizeExtraSmall
		    text: dist >= 1000 ? "at " + (dist / 1000).toFixed(1) + " km" : "at " + dist.toFixed(0) + " m"
		    color: Theme.secondaryColor
		    anchors.right: parent.right
		    anchors.bottom: parent.bottom
		}
	    onClicked: { search.text = model.name
	    		 drawer.open = false
			 map.setLookAt(model.coordinate.latitude, model.coordinate.longitude) } 
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
                title: map.wiki_entry.title
            }
	    Item {
		id: thumbnail
		visible: (map.wiki_entry.thumbnail != "")
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
       		    source: map.wiki_entry.thumbnail
       		    sourceSize.width: 360
       		    sourceSize.height: 360
    		}
            }
	    Label {
	        property real dist: map.coordinate.distanceTo(map.wiki_entry.coordinate)
		property string at: dist >= 1000 ? "at " + (dist / 1000).toFixed(1) + " km" : "at " + dist.toFixed(0) + " m"
		id: coordinates
		anchors.top: thumbnail.bottom
		anchors.right: parent.right
		anchors.left: parent.left
		anchors.rightMargin: Theme.paddingMedium
		text: "coordinates: " + map.wiki_entry.coordinateToString() + "\n" + at
		color: Theme.secondaryColor
		font.pixelSize: Theme.fontSizeExtraSmall
		horizontalAlignment: Text.AlignRight
	    }
	    Label {
                id: body
                text: map.wiki_entry.summary
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
		onClicked: { pageStack.pushAttached(wikipedia); pageStack.navigateForward() }
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
                title: map.wiki_entry.title
            }
            SilicaWebView {
		id: webView
		anchors {
		    top: wikititle.bottom
		    bottom: parent.bottom
		}
		width: page.width
		opacity: 0
        	onLoadingChanged: {
            	    switch (loadRequest.status)
            	    {
            	    case WebView.LoadSucceededStatus:
                        opacity = 1
                        break
           	    case WebView.LoadFailedStatus:
                        opacity = 0
                        viewPlaceHolder.errorString = loadRequest.errorString
                        break
	            default:
                        opacity = 0
                        break
                    }
                }

                FadeAnimation on opacity {}
	    }
	    ViewPlaceholder {
        	id: viewPlaceHolder
        	property string errorString

        	enabled: webView.opacity == 0
        	text: webView.loading ? "Loading" : "Web content load error: " + errorString
        	hintText: map.wiki_entry.url
	    }
	    Component.onCompleted: {
        	webView.url = map.wiki_entry.url
            }
        }
    }
}
