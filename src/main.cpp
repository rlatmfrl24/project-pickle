#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QVariant>

#include "app/AppController.h"
#include "media/MediaLibraryModel.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("Pickle"));
    QCoreApplication::setOrganizationName(QStringLiteral("Pickle"));

    MediaLibraryModel mediaLibraryModel;
    AppController appController(&mediaLibraryModel);

    QQmlApplicationEngine engine;
    engine.setInitialProperties({
        {QStringLiteral("mediaLibraryModel"), QVariant::fromValue(&mediaLibraryModel)},
        {QStringLiteral("appController"), QVariant::fromValue(&appController)},
    });

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.loadFromModule("pickle", "Main");

    return QCoreApplication::exec();
}
