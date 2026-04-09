#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include "shortcutlayoutfix.h"
#include "ozonscraper.h"
#include "productmodel.h"
#include "product.h"
#include "settingsservice.h"


int main(int argc, char* argv[])
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("ozon_cpp");
    QCoreApplication::setApplicationName("ozon_cpp");

    ShortcutLayoutFixFilter shortcutLayoutFix(&app);
    app.installEventFilter(&shortcutLayoutFix);

    OzonScraper scraper;
    ProductModel productModel;
    SettingsService settings;

    QObject::connect(&scraper, &OzonScraper::topProductsUpdated,
                     [&productModel](const QVector<Product>& products, int totalCount) {
                         productModel.setProducts(products, totalCount);
                     });

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("scraper", &scraper);
    engine.rootContext()->setContextProperty("productModel", &productModel);
    engine.rootContext()->setContextProperty("settings", &settings);

    engine.load(QUrl(QStringLiteral("qrc:/qml/main.qml")));

    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}
