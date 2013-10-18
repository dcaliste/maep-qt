import QtQuick 2.0
import Sailfish.Silica 1.0
import Maep 1.0
import QtWebKit 3.0
//import "pages"

ApplicationWindow
{
    initialPage: Page {
    id: page

    // To enable PullDownMenu, place our content in a SilicaFlickable
    SilicaFlickable {
        id: header
        anchors.top: parent.top

        // PullDownMenu and PushUpMenu must be declared in SilicaFlickable, SilicaListView or SilicaGridView
        PullDownMenu {
            MenuItem {
                text: "Show Page 2"
                //onClicked: pageStack.push(Qt.resolvedUrl("SecondPage.qml"))
            }
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
                    map.setSearchRequest(text)
		}
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
        onSearchRequest: { search.focus = true; search.label = "Place search" }
        onWikiURLSelected: { pageStack.push(wikipedia) }
	onWikiStatusChanged: { wikicheck.checked = status }
        onSearchResults: { placeview.visible = true; busy.running = false; search.label = search_results.length + " place(s) found" }
    }
    /*ListModel {
        id: placemodel
        model: map.getSearchResults()
    }*/
    SilicaListView {
        id: placeview
        z: 0
        width: parent.width * 0.75
	height: parent.height
	anchors.top: header.bottom
	anchors.horizontalCenter: parent.horizontalCenter
        visible: false
        model: map.search_results

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
        }
    }    
    }
    //cover: Qt.resolvedUrl("cover/CoverPage.qml")    

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
        }
    }
}
