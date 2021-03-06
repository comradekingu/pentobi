//-----------------------------------------------------------------------------
/** @file pentobi/qml/ButtonOk.qml
    @author Markus Enzenberger
    @copyright GNU General Public License version 3 or later */
//-----------------------------------------------------------------------------

import QtQuick.Controls 2.2
import "Controls.js" as PentobiControls

Button {
    text: qsTr("OK")
    DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
}
