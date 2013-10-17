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

Rectangle {
  id: page
  GpsMap {
    width: page.width
    height: page.height
  }
}