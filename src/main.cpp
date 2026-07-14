#include <auroraapp.h>
#include <QtQuick>
#include "imagecontroller.h"
#include "aiimageprovider.h"

int main(int argc, char *argv[])
{
    qRegisterMetaType<QImage>("QImage");

    QScopedPointer<QGuiApplication> application(Aurora::Application::application(argc, argv));
    QScopedPointer<QQuickView> view(Aurora::Application::createView());

    application->setOrganizationName(QStringLiteral("ru.omgtu"));
    application->setApplicationName(QStringLiteral("PhotoRora"));

    ImageController imageController;
    AiImageProvider aiProvider;

    imageController.setProvider(&aiProvider);

    view->rootContext()->setContextProperty(QStringLiteral("ImageController"), &imageController);
    view->engine()->addImageProvider(QLatin1String("ai_provider"), &aiProvider);

    view->setSource(Aurora::Application::pathTo(QStringLiteral("qml/PhotoRora.qml")));
    view->show();

    return application->exec();
}
