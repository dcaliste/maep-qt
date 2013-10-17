import QtQuick 2.0
import Sailfish.Silica 1.0
//import QtGraphicalEffects 1.0
import Maep 1.0
//import "pages"

ApplicationWindow
{
    initialPage: Page {
    id: page

    //Column {
    // To enable PullDownMenu, place our content in a SilicaFlickable
    SilicaFlickable {
        id: title
        anchors.top: parent.top

        // PullDownMenu and PushUpMenu must be declared in SilicaFlickable, SilicaListView or SilicaGridView
        PullDownMenu {
            MenuItem {
                text: "Show Page 2"
                //onClicked: pageStack.push(Qt.resolvedUrl("SecondPage.qml"))
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
            PageHeader {
                title: "Mæp"
            }
    }
/*    DropShadow {
        anchors.fill: title
        cached: true
        horizontalOffset: 3
        verticalOffset: 3
        radius: 8.0
        samples: 16
        color: "#80000000"
        source: title
    }*/
            /*Label {
                x: Theme.paddingLarge
                text: "Hello Sailors"
                color: Theme.secondaryHighlightColor
                font.pixelSize: Theme.fontSizeLarge
            }*/
        GpsMap {
	   id: map
           anchors.top: title.bottom
           //anchors.bottom: parent.bottom
           width: page.width
	   z: -1
	//      width: page.width
           height: page.height - title.height
    //}
    //}
    }
    }
    //cover: Qt.resolvedUrl("cover/CoverPage.qml")
    
}
/*import QtQuick 2.0
import Maep 1.0

Rectangle {
  id: page
  GpsMap {
    width: page.width
    height: page.height
  }
}*/
