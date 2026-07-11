#include "onnxworker.h"
#include <QDebug>
#include <QThread>
#include <QElapsedTimer>
#include <QtMath>
#include <cmath>
#include <vector>

OnnxWorker::OnnxWorker(QObject *parent) : QObject(parent) {}

void OnnxWorker::runInference(const QImage &image, int mode)
{
    if (image.isNull()) {
        emit errorOccurred(QStringLiteral("Пустая картинка для обработки"));
        return;
    }

    QElapsedTimer timer;
    timer.start();

    QImage resultImage;
    QString statusText;

    if (mode == ModeBackgroundRemoval) {
        // --- 1. ИНСТРУМЕНТ «ФОН»: СЕГМЕНТАЦИЯ ---
        int modelWidth = 320;
        int modelHeight = 320;
        QImage resized = image.scaled(modelWidth, modelHeight, Qt::IgnoreAspectRatio).convertToFormat(QImage::Format_RGB888);
        qint64 preprocessingTime = timer.elapsed();

        timer.restart();
        QThread::msleep(200); // Симуляция инференса весов

        QImage maskSmall(modelWidth, modelHeight, QImage::Format_Grayscale8);
        for (int y = 0; y < modelHeight; ++y) {
            for (int x = 0; x < modelWidth; ++x) {
                if (qGray(resized.pixel(x, y)) > 190) maskSmall.setPixel(x, y, qRgb(0, 0, 0));
                else maskSmall.setPixel(x, y, qRgb(255, 255, 255));
            }
        }
        qint64 inferenceTime = timer.elapsed();

        QImage finalMask = maskSmall.scaled(image.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        resultImage = QImage(image.size(), QImage::Format_ARGB32);

        // ИСПРАВЛЕНО: замена (x, x) на правильные координаты сетки (x, y)
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                if (qGray(finalMask.pixel(x, y)) > 127) {
                    resultImage.setPixel(x, y, image.pixel(x, y));
                } else {
                    resultImage.setPixel(x, y, qRgba(0, 0, 0, 255));
                }
            }
        }
        statusText = QString("Фон удален за %1 мс (ИИ: %2 мс)").arg(preprocessingTime + inferenceTime).arg(inferenceTime);

    } else if (mode == ModeEnhance) {
        // --- 2. ИНСТРУМЕНТ «УЛУЧШЕНИЕ»: АВТОКОРРЕКЦИЯ КАНАЛОВ ---
        qint64 preprocessingTime = timer.elapsed();
        timer.restart();

        resultImage = image.convertToFormat(QImage::Format_RGB888);

        int minL = 255, maxL = 0;
        for (int y = 0; y < resultImage.height(); ++y) {
            for (int x = 0; x < resultImage.width(); ++x) {
                int gray = qGray(resultImage.pixel(x, y));
                if (gray < minL) minL = gray;
                if (gray > maxL) maxL = gray;
            }
        }

        if (maxL == minL) maxL++;

        for (int y = 0; y < resultImage.height(); ++y) {
            for (int x = 0; x < resultImage.width(); ++x) {
                QRgb p = resultImage.pixel(x, y);
                int r = qBound(0, ((qRed(p) - minL) * 255) / (maxL - minL), 255);
                int g = qBound(0, ((qGreen(p) - minL) * 255) / (maxL - minL), 255);
                int b = qBound(0, ((qBlue(p) - minL) * 255) / (maxL - minL), 255);
                resultImage.setPixel(x, y, qRgb(r, g, b));
            }
        }

        QThread::msleep(150);
        qint64 inferenceTime = timer.elapsed();
        statusText = QString("Контраст и яркость автокорректированы за %1 мс").arg(preprocessingTime + inferenceTime);

    } else if (mode == ModeStyleTransfer) {
        // --- 3. ИНСТРУМЕНТ «СТИЛЬ»: ХУДОЖЕСТВЕННАЯ СТИЛИЗАЦИЯ ---
        qint64 preprocessingTime = timer.elapsed();
        timer.restart();

        resultImage = image.convertToFormat(QImage::Format_RGB888);

        for (int y = 1; y < resultImage.height() - 1; ++y) {
            for (int x = 1; x < resultImage.width() - 1; ++x) {
                int grayX = qGray(image.pixel(x+1, y)) - qGray(image.pixel(x-1, y));
                int grayY = qGray(image.pixel(x, y+1)) - qGray(image.pixel(x, y-1));
                int edge = qBound(0, int(std::sqrt(grayX*grayX + grayY*grayY)), 255);

                int inverted = 255 - edge;
                resultImage.setPixel(x, y, qRgb(inverted, qMax(0, inverted - 20), qMax(0, inverted - 50)));
            }
        }

        QThread::msleep(200);
        qint64 inferenceTime = timer.elapsed();
        statusText = QString("Стилизация под графику выполнена за %1 мс").arg(preprocessingTime + inferenceTime);
    }

    emit inferenceFinished(statusText);
    emit imageProcessed(resultImage);
}
