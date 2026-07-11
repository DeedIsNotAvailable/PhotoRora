#include <auroraapp.h>
#include <QtQuick>
#include "imagecontroller.h"
#include "aiimageprovider.h"

int main(int argc, char *argv[])
{
    qRegisterMetaType<QImage>("QImage");

    // ТЗ ОФЛАЙН ИНФЕРЕНС: Динамически загружаем ядро ONNX Runtime из изолированной папки приложения
    QString libPath = QStringLiteral("/usr/share/ru.omgtu.PhotoRora/lib/libonnxruntime.so");
    QLibrary onnxLib(libPath);
    if (!onnxLib.load()) {
        qWarning() << "[ИИ Ошибка] Не удалось загрузить ядро ONNX Runtime в песочнице:" << onnxLib.errorString();
    } else {
        qDebug() << "[ИИ Успех] Библиотека ONNX Runtime успешно инициализирована в рантайме!";
    }

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
