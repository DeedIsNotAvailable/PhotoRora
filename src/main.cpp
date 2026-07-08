#include <auroraapp.h>
#include <QtQuick>
#include "imagecontroller.h"

int main(int argc, char *argv[])
{
    QScopedPointer<QGuiApplication> application(Aurora::Application::application(argc, argv));
    QScopedPointer<QQuickView> view(Aurora::Application::createView());

    // Создаем экземпляр контроллера
    ImageController imageController;

    // Регистрируем его в контексте QML
    view->rootContext()->setContextProperty("ImageController", &imageController);

    view->setSource(Aurora::Application::pathTo("qml/PhotoRora.qml"));

    view->show();

    return application->exec();
}
