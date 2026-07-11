#include <auroraapp.h>
#include <QtQuick>
#include "imagecontroller.h"

int main(int argc, char *argv[])
{
    QScopedPointer<QGuiApplication> application(Aurora::Application::application(argc, argv));
    QScopedPointer<QQuickView> view(Aurora::Application::createView());

    application->setOrganizationName(QStringLiteral("ru.omgtu"));
    application->setApplicationName(QStringLiteral("PhotoRora"));

    ImageController imageController;

    view->rootContext()->setContextProperty("ImageController", &imageController);

    view->setSource(Aurora::Application::pathTo("qml/PhotoRora.qml"));

    view->show();

    return application->exec();
}
