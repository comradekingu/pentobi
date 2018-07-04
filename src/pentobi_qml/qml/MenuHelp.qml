import QtQuick 2.0
import QtQuick.Controls 2.2
import "Main.js" as Logic
import "." as Pentobi

Pentobi.Menu {
    title: Logic.removeShortcut(qsTr("&Help"))

    MenuItem { action: actions.actionHelp }
    MenuItem {
        text: Logic.removeShortcut(qsTr("&About Pentobi"))
        onTriggered: Logic.about()
    }
}
