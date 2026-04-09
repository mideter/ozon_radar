import QtQml 2.15
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

ApplicationWindow {
    id: root
    width: 850
    height: 1000
    visible: true
    title: "OzonRadar"

    function formatPrice(p) {
        if (typeof p !== "number" || p <= 0) return "—"
        var s = Math.floor(p).toString()
        return s.replace(/\B(?=(\d{3})+(?!\d))/g, '\u2009') + " ₽"
    }
    readonly property string white: "#ffffff"
    readonly property string gray100: "#f7fafc"
    readonly property string gray200: "#e3e8f0"
    readonly property string gray700: "#334155"
    readonly property string gray800: "#1e293b"
    readonly property string gray500: "#64748b"
    readonly property string gray50: "#f8fafc"
    readonly property string blue600: "#2563eb"
    readonly property string blue500: "#3b82f6"
    readonly property string green600: "#059669"
    readonly property string blue400: "#0ea5e9"
    readonly property string red600: "#dc2626"
    readonly property string blue50: "#dbeafe"

    property string errorMessage: ""
    property string statusText1: "Готов к работе"
    property string statusText2: ""
    property string statusColor: "#059669"
    property bool isRunning: false
    property int productsFoundCount: -1

    function openSettingsWindow() {
        settingsWindowLoader.active = true
        if (settingsWindowLoader.item) {
            settingsWindowLoader.item.parserIsRunning = root.isRunning
            settingsWindowLoader.item.visible = true
            settingsWindowLoader.item.raise()
            settingsWindowLoader.item.requestActivate()
        }
    }

    background: Rectangle {
        color: root.gray100
    }

    Connections {
        target: scraper

        function onStatusChanged(msg, count, lastPrice) {
            // count >= 0 — статус с количеством товаров; иначе — простой текст (загрузка и т.п.)
            if (count >= 0) {
                productsFoundCount = count
                statusText1 = msg
                statusText2 = (typeof lastPrice === "number" && lastPrice > 0) ? (" | Цена последнего товара: " + root.formatPrice(lastPrice)) : ""
            } else {
                statusText1 = msg
                statusText2 = ""
            }
            statusColor = "#0EA5E9"
        }

        function linksWordCountSuffix(urlCount) {
            if (typeof urlCount !== "number" || urlCount <= 1)
                return ""
            var n10 = urlCount % 10
            var n100 = urlCount % 100
            var w
            if (n10 === 1 && n100 !== 11)
                w = "ссылка"
            else if (n10 >= 2 && n10 <= 4 && (n100 < 10 || n100 >= 20))
                w = "ссылки"
            else
                w = "ссылок"
            return " (" + urlCount + " " + w + ")"
        }

        function onFinishedSuccessfully(totalCount, elapsedText, urlCount) {
            isRunning = false
            progressBar.visible = false
            parseButton.enabled = true
            statusColor = "#059669"
            var n = (typeof totalCount === "number" && totalCount >= 0)
                ? totalCount
                : Math.max(0, productModel.totalCount)
            statusText1 = "Успешно загружено " + n + " товаров" + linksWordCountSuffix(urlCount)
            statusText2 = elapsedText
        }

        function onFinishedWithError(msg) {
            isRunning = false
            progressBar.visible = false
            parseButton.enabled = true
            statusColor = "#DC2626"
            statusText1 = "Ошибка при загрузке"
            statusText2 = ""
            errorMessage = msg
            errorDialog.open()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 12

        // Действия
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: actionsColumn.implicitHeight + 24
            color: root.white
            border.color: root.gray200
            radius: 6

            ColumnLayout {
                id: actionsColumn
                anchors.fill: parent
                anchors.margins: 16
                anchors.topMargin: 12
                anchors.bottomMargin: 12
                spacing: 8

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    Text {
                        text: "Параметры запуска задаются в отдельном окне настроек."
                        Layout.fillWidth: true
                        color: root.gray800
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Button {
                        id: settingsButton
                        text: "Настройки..."
                        onClicked: openSettingsWindow()
                    }
                    Item { Layout.fillWidth: true }
                    Button {
                        id: parseButton
                        text: "Загрузить товары"
                        background: Rectangle {
                            color: root.blue500
                            radius: 4
                        }
                        contentItem: Text {
                            text: parseButton.text
                            color: root.white
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: startParsing()
                    }
                }
            }
        }

        // Статус
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            color: root.white
            border.color: root.gray200
            radius: 4

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 8

                Text {
                    text: root.isRunning ? "⟳" : (root.statusColor === "#059669" ? "✓" : (root.statusColor === "#DC2626" ? "✕" : "●"))
                    color: root.statusColor === "#059669" ? root.green600 : (root.statusColor === "#DC2626" ? root.red600 : root.blue400)
                }
                Text {
                    id: statusLabel
                    text: root.statusText1
                    Layout.fillWidth: true
                    color: root.gray800
                    wrapMode: Text.WordWrap
                }
                Text {
                    text: root.statusText2
                    Layout.fillWidth: true
                    color: root.gray800
                    wrapMode: Text.WordWrap
                }
            }
        }

        // Прогресс
        Item {
            id: progressBar
            Layout.fillWidth: true
            Layout.preferredHeight: 8
            visible: false
            clip: true

            Rectangle {
                anchors.fill: parent
                color: root.blue50
                radius: 4
            }
            Rectangle {
                id: progressIndicator
                width: Math.max(40, parent.width * 0.25)
                height: parent.height
                color: root.blue500
                radius: 4
                x: 0

                SequentialAnimation on x {
                    running: progressBar.visible && progressBar.width > progressIndicator.width
                    loops: Animation.Infinite
                    NumberAnimation {
                        to: Math.max(0, progressBar.width - progressIndicator.width)
                        duration: 1200
                        easing.type: Easing.InOutQuad
                    }
                    NumberAnimation {
                        to: 0
                        duration: 1200
                        easing.type: Easing.InOutQuad
                    }
                }
            }
        }

        // Результаты
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: root.white
            border.color: root.gray200
            radius: 6

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 8

                Text {
                    text: "Результаты"
                    color: root.gray700
                }

                ListView {
                    id: productList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: productModel
                    spacing: 0

                    property int colNo: 60
                    property int colName: Math.max(200, productList.width - productList.colNo - productList.colPrice - productList.colPoints - productList.colRatio)
                    property int colPrice: 100
                    property int colPoints: 110
                    property int colRatio: 90

                    header: Row {
                        width: productList.width
                        height: 40
                        spacing: 0

                        Rectangle {
                            width: productList.colNo
                            height: 40
                            color: root.blue600
                            Text {
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 8
                                text: "№"
                                color: root.white
                                verticalAlignment: Text.AlignVCenter
                            }
                        }
                        Rectangle {
                            width: productList.colName
                            height: 40
                            color: root.blue600
                            Text {
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 8
                                text: "Название товара"
                                color: root.white
                                verticalAlignment: Text.AlignVCenter
                            }
                        }
                        Rectangle {
                            width: productList.colPrice
                            height: 40
                            color: root.blue600
                            Text {
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 8
                                text: "Цена"
                                color: root.white
                                verticalAlignment: Text.AlignVCenter
                            }
                        }
                        Rectangle {
                            width: productList.colPoints
                            height: 40
                            color: root.blue600
                            Text {
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 8
                                text: "Баллы за отзыв"
                                color: root.white
                                verticalAlignment: Text.AlignVCenter
                            }
                        }
                        Rectangle {
                            width: productList.colRatio
                            height: 40
                            color: root.blue600
                            Text {
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 8
                                text: "Баллы/Цена"
                                color: root.white
                                verticalAlignment: Text.AlignVCenter
                            }
                        }
                    }

                    delegate: MouseArea {
                        width: productList.width
                        height: 44
                        hoverEnabled: true

                        Row {
                            width: parent.width
                            height: parent.height
                            spacing: 0

                            Rectangle {
                                width: productList.colNo
                                height: 44
                                color: (typeof index !== "undefined" && index % 2 === 0) ? root.white : root.gray100
                                Text {
                                    anchors.fill: parent
                                    anchors.leftMargin: 8
                                    anchors.rightMargin: 8
                                    text: model.index
                                    color: root.gray800
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }
                            Rectangle {
                                width: productList.colName
                                height: 44
                                color: (typeof index !== "undefined" && index % 2 === 0) ? root.white : root.gray100
                                Text {
                                    anchors.fill: parent
                                    anchors.leftMargin: 8
                                    anchors.rightMargin: 8
                                    text: model.name
                                    color: root.gray800
                                    elide: Text.ElideRight
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }
                            Rectangle {
                                width: productList.colPrice
                                height: 44
                                color: (typeof index !== "undefined" && index % 2 === 0) ? root.white : root.gray100
                                Text {
                                    anchors.fill: parent
                                    anchors.leftMargin: 8
                                    anchors.rightMargin: 8
                                    text: root.formatPrice(model.price)
                                    color: root.gray800
                                    elide: Text.ElideRight
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }
                            Rectangle {
                                width: productList.colPoints
                                height: 44
                                color: (typeof index !== "undefined" && index % 2 === 0) ? root.white : root.gray100
                                Text {
                                    anchors.fill: parent
                                    anchors.leftMargin: 8
                                    anchors.rightMargin: 8
                                    text: model.reviewPoints === 0 ? "—" : model.reviewPoints
                                    color: root.gray800
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }
                            Rectangle {
                                width: productList.colRatio
                                height: 44
                                color: (typeof index !== "undefined" && index % 2 === 0) ? root.white : root.gray100
                                Text {
                                    anchors.fill: parent
                                    anchors.leftMargin: 8
                                    anchors.rightMargin: 8
                                    text: model.ratio
                                    color: root.gray800
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }
                        }

                        onClicked: {
                            if (model.url && model.url !== "")
                                Qt.openUrlExternally(model.url)
                        }
                    }
                }
            }
        }
    }

    Popup {
        id: errorDialog
        modal: true
        anchors.centerIn: parent
        width: Math.min(parent.width - 40, 400)
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        ColumnLayout {
            spacing: 16
            Text {
                text: "Ошибка"
                Layout.alignment: Qt.AlignHCenter
            }
            Text {
                text: root.errorMessage
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                Layout.preferredWidth: parent.width - 32
            }
            Button {
                text: "OK"
                Layout.alignment: Qt.AlignHCenter
                onClicked: errorDialog.close()
            }
        }
    }

    function startParsing() {
        var urlText = settings.urlsText
        var minPoints = settings.minPoints
        var maxPoints = settings.maxPoints
        var validationError = settings.validate(urlText, settings.minPointsText(), settings.maxPointsText())
        if (validationError !== "") {
            statusColor = "#DC2626"
            statusText1 = "Ошибка в настройках"
            statusText2 = ""
            errorMessage = validationError
            errorDialog.open()
            return
        }

        productModel.clear()
        productsFoundCount = -1
        parseButton.enabled = false
        progressBar.visible = true
        root.isRunning = true
        statusText1 = "Инициализация браузера и загрузка страницы..."
        statusText2 = ""
        statusColor = "#0EA5E9"

        scraper.start(urlText, minPoints, maxPoints)
    }

    Loader {
        id: settingsWindowLoader
        active: false
        source: "qrc:/qml/SettingsWindow.qml"
        onLoaded: {
            item.parserIsRunning = root.isRunning
            item.visible = true
            item.raise()
            item.requestActivate()
        }
    }

    onIsRunningChanged: {
        if (settingsWindowLoader.item)
            settingsWindowLoader.item.parserIsRunning = root.isRunning
    }
}
