#include "imagecontroller.h"
#include "aiimageprovider.h"
#include <QFileInfo>
#include <QDebug>
#include <QStandardPaths>
#include <QDateTime>

ImageController::ImageController(QObject *parent) : QObject(parent)
{
    m_worker = new OnnxWorker();
    m_worker->moveToThread(&m_workerThread);

    connect(this, &ImageController::startInference, m_worker, &OnnxWorker::runInference);
    connect(m_worker, &OnnxWorker::inferenceFinished, this, &ImageController::onInferenceFinished);
    connect(m_worker, &OnnxWorker::errorOccurred, this, &ImageController::onInferenceError);
    connect(m_worker, &OnnxWorker::inferenceCanceled, this, &ImageController::onInferenceCanceled);
    connect(m_worker, &OnnxWorker::imageProcessed, this, &ImageController::onImageProcessed);

    connect(&m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);
    m_workerThread.start();
}

ImageController::~ImageController()
{
    if (m_worker) {
        m_worker->requestCancel();
    }

    m_workerThread.quit();
    m_workerThread.wait();
}

void ImageController::setProvider(AiImageProvider *provider)
{
    m_provider = provider;
}

void ImageController::loadImage(const QString &filePath)
{
    if (filePath.isEmpty()) {
        emit errorOccurred(QStringLiteral("Путь к файлу пуст"));
        return;
    }

    QFileInfo checkFile(filePath);
    if (!checkFile.exists() || !checkFile.isFile()) {
        emit errorOccurred(QStringLiteral("Файл не найден"));
        return;
    }

    QImage img;
    if (!img.load(filePath)) {
        emit errorOccurred(QStringLiteral("Ошибка чтения графики"));
        return;
    }

    m_originalImage = img;
    m_history.clear();
    m_history.append(img);

    emit imageLoadedSuccessfully();
    emit historyChanged();

    updateUiWithCurrentImage();
}

void ImageController::triggerBackgroundRemoval()
{
    if (m_history.isEmpty() || m_isProcessing) return;

    m_isProcessing = true;
    m_aiResult = QStringLiteral("Нейросеть удаляет фон...");
    emit isProcessingChanged();
    emit aiResultChanged();

    emit startInference(m_history.last(), 0); // 0 -> ModeBackgroundRemoval
}

void ImageController::triggerEnhancement()
{
    if (m_history.isEmpty() || m_isProcessing) return;

    m_isProcessing = true;
    m_aiResult = QStringLiteral("Автокоррекция контраста и яркости...");
    emit isProcessingChanged();
    emit aiResultChanged();

    emit startInference(m_history.last(), 1); // 1 -> ModeEnhance
}

void ImageController::triggerStyleTransfer()
{
    if (m_history.isEmpty() || m_isProcessing) return;

    m_isProcessing = true;
    m_aiResult = QStringLiteral("Художественная стилизация изображения...");
    emit isProcessingChanged();
    emit aiResultChanged();

    emit startInference(m_history.last(), 2); // 2 -> ModeStyleTransfer
}

void ImageController::undo()
{
    if (m_history.size() > 1) {
        m_history.removeLast();
        m_aiResult = QStringLiteral("Последнее действие отменено");
        emit aiResultChanged();
        emit historyChanged();
        updateUiWithCurrentImage();
    }
}

void ImageController::resetToOriginal()
{
    if (m_originalImage.isNull()) return;

    m_history.clear();
    m_history.append(m_originalImage);

    m_aiResult = QStringLiteral("Изображение сброшено к оригиналу");
    emit aiResultChanged();
    emit historyChanged();
    updateUiWithCurrentImage();
}

void ImageController::cancelProcessing()
{
    if (!m_isProcessing || !m_worker) {
        return;
    }

    m_worker->requestCancel();
    m_aiResult = QStringLiteral("Запрошена отмена текущей обработки...");
    emit aiResultChanged();
}

void ImageController::onInferenceFinished(const QString &result)
{
    m_isProcessing = false;
    m_aiResult = result;
    emit isProcessingChanged();
    emit aiResultChanged();
}

void ImageController::onInferenceCanceled(const QString &message)
{
    m_isProcessing = false;
    m_aiResult = message;
    emit isProcessingChanged();
    emit aiResultChanged();
}

void ImageController::onInferenceError(const QString &message)
{
    m_isProcessing = false;
    m_aiResult = QStringLiteral("Ошибка: ") + message;
    emit isProcessingChanged();
    emit aiResultChanged();
}

void ImageController::onImageProcessed(const QImage &processedImage)
{
    m_history.append(processedImage);
    emit historyChanged();
    updateUiWithCurrentImage();
}

void ImageController::updateUiWithCurrentImage()
{
    if (m_provider && !m_history.isEmpty()) {
        m_provider->setImage(m_history.last());
        emit contourReady();
    }
}

void ImageController::exportResult()
{
    if (m_history.isEmpty()) return;

    // Получаем путь к официальной папке Pictures внутри песочницы Авроры
    QString picturesPath = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (picturesPath.isEmpty()) picturesPath = QStringLiteral("/home/defaultuser/Pictures");

    // Формируем уникальное имя файла по дате и времени сохранения
    QString fileName = QString("/Result_%1.png").arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));
    QString fullPath = picturesPath + fileName;

    // ТЗ: Экспорт результата на диск в исходном (оригинальном) разрешении кадра
    if (m_history.last().save(fullPath, "PNG")) {
        m_aiResult = QStringLiteral("Успешно сохранено в Галерею: ") + fileName;
    } else {
        m_aiResult = QStringLiteral("Ошибка сохранения файла на диск");
    }
    emit aiResultChanged();
}
