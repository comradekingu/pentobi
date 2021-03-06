//-----------------------------------------------------------------------------
/** @file pentobi/qml/MenuView.qml
    @author Markus Enzenberger
    @copyright GNU General Public License version 3 or later */
//-----------------------------------------------------------------------------

import QtQuick.Controls 2.3
import "." as Pentobi

Pentobi.Menu {
    title: addMnemonic(qsTr("View"),
                       //: Mnemonic for menu View. Leave empty for no mnemonic.
                       qsTr("V"))

    Action {
        text: addMnemonic(qsTr("Appearance…"),
                          //: Mnemonic for menu Appearance. Leave empty for no mnemonic.
                          qsTr("A"))
        onTriggered: appearanceDialog.open()
    }
    Pentobi.MenuItem {
        action: actions.comment
        text: addMnemonic(action.text,
                          //: Mnemonic for menu item View/Comment. Leave empty for no mnemonic.
                          qsTr("C"))
    }
    Pentobi.MenuItem {
        action: actions.fullscreen
        text: addMnemonic(action.text,
                          //: Mnemonic for menu item Fullscreen. Leave empty for no mnemonic.
                          qsTr("F"))
    }
}
