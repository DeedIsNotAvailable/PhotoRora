#include "imagecontroller.h"
#include <QFileInfo>

ImageController::ImageController(QObject *parent) : QObject(parent) {}

void ImageController::loadImage(const QString &filePath)
{
    if (filePath.isEmpty()) {
        emit errorOccurred("Путь к файлу пуст");
        return;
    }

    // Проверяем существование файла (учитывая песочницу ОС Аврора)
    QFileInfo checkFile(filePath);
    if (!checkFile.exists() || !checkFile.isFile()) {
        emit errorOccurred("Файл не найден или недоступен");
        return;
    }

    // Загружаем изображение
    QImage img;
    if (!img.load(filePath)) {
        emit errorOccurred("Не удалось загрузить изображение через QImage");
        return;
    }

    // Сохраняем во внутреннее свойство для инференса
    m_currentImage = img;

    qDebug() << "Изображение успешно импортировано. Размер:" << m_currentImage.size();
    emit imageLoadedSuccessfully();

    // ДАЛЬШЕ: Здесь вы можете вызвать метод вашего OnnxRunner,
    // предварительно преобразовав QImage в нужный для нейросети формат (RGB, Float32 и т.д.)
}
