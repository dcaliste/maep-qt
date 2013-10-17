/*import QtQuick 2.0
//import Sailfish.Silica 1.0
//import "pages"

//ApplicationWindow
Rectangle
{
    //initialPage: AnalogClock { }
    //cover: Qt.resolvedUrl("cover/CoverPage.qml")
    
}*/
import QtQuick 2.0
import Maep 1.0
import QtWebKit 3.0

Rectangle {
  id: page
  Column {
    visible: true
    TextInput {
      id: search
      visible: false
      width: page.width
      text: "Enter a place name"
      font.pointSize: 24
      onAccepted: {
        map.setSearchRequest(text)
      }
    }
    GpsMap {
      id: map
      width: page.width
      height: page.height
      onWikiURLSelected: {
        map.visible = false
        wikipedia.visible = true
        wikipediaTitle.text = title
        wikipediaPage.url = url
      }
      onSearchRequest: {
        search.visible = true
        height = page.height - search.height
      }
    }
  }
  Item {
    id: wikipedia
    visible: false
    Text {
      id: wikipediaTitle
      font.pointSize: 24
      MouseArea {
        anchors.fill: parent
        onClicked: {
          map.visible = true
          wikipedia.visible = false
        }
      }
    }
    WebView {
      id: wikipediaPage
      anchors.top: wikipediaTitle.bottom
      width: page.width
      height: page.height - wikipedia.height
    }
  }
}

