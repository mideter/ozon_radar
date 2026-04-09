import QtQml 2.15
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

ApplicationWindow {
    id: settingsWindow
    width: 760
    height: 520
    visible: false
    title: "Настройки"

    property bool parserIsRunning: false

    readonly property string white: "#ffffff"
    readonly property string gray100: "#f7fafc"
    readonly property string gray200: "#e3e8f0"
    readonly property string gray700: "#334155"
    readonly property string gray800: "#1e293b"
    readonly property string gray500: "#64748b"
    readonly property string gray50: "#f8fafc"
    readonly property string blue500: "#3b82f6"
    readonly property string blue600: "#2563eb"
    readonly property string amber600: "#d97706"
    readonly property string red600: "#dc2626"

    property string validationError: ""

    background: Rectangle {
        color: settingsWindow.gray100
    }

    function syncFromSettings() {
        urlsEdit.text = settings.urlsText
        minPointsEdit.text = settings.minPointsText()
        maxPointsEdit.text = settings.maxPointsText()
        validationError = ""
    }

    function trySave() {
        var error = settings.validate(urlsEdit.text, minPointsEdit.text, maxPointsEdit.text)
        if (error !== "") {
            validationError = error
            return
        }
        settings.applyAndSave(urlsEdit.text, minPointsEdit.text, maxPointsEdit.text)
        validationError = ""
        close()
    }

    onVisibleChanged: {
        if (visible)
            syncFromSettings()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 10

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: hintText.implicitHeight + 16
            color: settingsWindow.white
            border.color: settingsWindow.gray200
            radius: 6

            Text {
                id: hintText
                anchors.fill: parent
                anchors.margins: 8
                wrapMode: Text.WordWrap
                color: settingsWindow.gray700
                text: settingsWindow.parserIsRunning
                      ? "Парсинг уже выполняется. Сохраненные изменения будут применены к следующему запуску."
                      : "Изменения применяются к следующему запуску парсинга."
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: settingsWindow.white
            border.color: settingsWindow.gray200
            radius: 6

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8

                Text {
                    text: "Ссылки (по одной в строке)"
                    color: settingsWindow.gray800
                }

                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true

                    TextArea {
                        id: urlsEdit
                        anchors.left: parent.left
                        anchors.right: parent.right
                        wrapMode: TextArea.Wrap
                        color: settingsWindow.gray800
                        selectByMouse: true
                        background: Rectangle {
                            color: settingsWindow.gray50
                            border.color: settingsWindow.gray200
                            radius: 4
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Text {
                        text: "Баллы за отзыв"
                        color: settingsWindow.gray800
                    }

                    TextField {
                        id: minPointsEdit
                        Layout.preferredWidth: 90
                        placeholderText: "мин"
                        background: Rectangle {
                            color: settingsWindow.gray50
                            border.color: settingsWindow.gray200
                            radius: 4
                        }
                    }

                    Text {
                        text: "—"
                        color: settingsWindow.gray500
                    }

                    TextField {
                        id: maxPointsEdit
                        Layout.preferredWidth: 90
                        placeholderText: "макс"
                        background: Rectangle {
                            color: settingsWindow.gray50
                            border.color: settingsWindow.gray200
                            radius: 4
                        }
                    }
                    Item { Layout.fillWidth: true }
                }

                Text {
                    Layout.fillWidth: true
                    text: validationError
                    visible: validationError !== ""
                    color: settingsWindow.red600
                    wrapMode: Text.WordWrap
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Button {
                id: resetButton
                text: "Сбросить"
                onClicked: {
                    settings.resetDefaults()
                    syncFromSettings()
                }
                background: Rectangle {
                    color: settingsWindow.amber600
                    radius: 4
                }
                contentItem: Text {
                    text: resetButton.text
                    color: settingsWindow.white
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }

            Item { Layout.fillWidth: true }

            Button {
                text: "Отмена"
                onClicked: close()
            }

            Button {
                id: saveButton
                text: "Сохранить"
                onClicked: trySave()
                background: Rectangle {
                    color: settingsWindow.blue500
                    radius: 4
                }
                contentItem: Text {
                    text: saveButton.text
                    color: settingsWindow.white
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }
}
