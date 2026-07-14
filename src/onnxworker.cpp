#include "onnxworker.h"
#include <onnxruntime_cxx_api.h>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QColor>
#include <QThread>
#include <QElapsedTimer>
#include <QtMath>
#include <cmath>
#include <vector>

OnnxWorker::OnnxWorker(QObject *parent) : QObject(parent) {}

OnnxWorker::~OnnxWorker() = default;

namespace {
int effectiveDimension(qint64 value, int fallback)
{
    return value > 0 ? static_cast<int>(value) : fallback;
}

float toProbability(float value)
{
    if (value >= 0.0f && value <= 1.0f) {
        return value;
    }

    return 1.0f / (1.0f + std::exp(-value));
}

QImage makeBlurredBackdrop(const QImage &source)
{
    const QImage rgbSource = source.convertToFormat(QImage::Format_RGB32);
    const int blurWidth = qMax(1, source.width() / 18);
    const int blurHeight = qMax(1, source.height() / 18);
    return rgbSource.scaled(blurWidth, blurHeight, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
        .scaled(source.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
        .convertToFormat(QImage::Format_ARGB32);
}
}

void OnnxWorker::requestCancel()
{
    m_cancelRequested.store(true);
}

QString OnnxWorker::resolveModelPath() const
{
    const QStringList candidates = {
        QStringLiteral("/usr/share/ru.omgtu.PhotoRora/lib/bgremoval.onnx"),
        QDir::cleanPath(QCoreApplication::applicationDirPath() + QStringLiteral("/../share/ru.omgtu.PhotoRora/lib/bgremoval.onnx")),
        QDir::cleanPath(QCoreApplication::applicationDirPath() + QStringLiteral("/../../share/ru.omgtu.PhotoRora/lib/bgremoval.onnx")),
        QDir::cleanPath(QDir::currentPath() + QStringLiteral("/data/bgremoval.onnx"))
    };

    for (const QString &candidate : candidates) {
        if (QFileInfo::exists(candidate)) {
            return candidate;
        }
    }

    return candidates.first();
}

bool OnnxWorker::ensureBackgroundSession(QString &errorMessage)
{
    if (m_backgroundSessionReady) {
        return true;
    }

    const QString modelPath = resolveModelPath();
    if (!QFileInfo::exists(modelPath)) {
        errorMessage = QStringLiteral("Не найден файл модели: %1").arg(modelPath);
        return false;
    }

    try {
        m_env = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "PhotoRora");
        m_sessionOptions = std::make_unique<Ort::SessionOptions>();
        m_sessionOptions->SetIntraOpNumThreads(1);
        m_sessionOptions->SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_EXTENDED);

        const std::string modelPathUtf8 = modelPath.toStdString();
        m_backgroundSession = std::make_unique<Ort::Session>(*m_env, modelPathUtf8.c_str(), *m_sessionOptions);
        m_runOptions = std::make_unique<Ort::RunOptions>();

        Ort::AllocatorWithDefaultOptions allocator;

        const size_t inputCount = m_backgroundSession->GetInputCount();
        const size_t outputCount = m_backgroundSession->GetOutputCount();
        if (inputCount == 0 || outputCount == 0) {
            errorMessage = QStringLiteral("Модель не содержит входов или выходов");
            return false;
        }

        m_inputNameStorage.clear();
        m_outputNameStorage.clear();
        m_inputNames.clear();
        m_outputNames.clear();
        m_inputNameStorage.reserve(inputCount);
        m_outputNameStorage.reserve(outputCount);
        m_inputNames.reserve(inputCount);
        m_outputNames.reserve(outputCount);

        for (size_t i = 0; i < inputCount; ++i) {
            auto inputName = m_backgroundSession->GetInputNameAllocated(i, allocator);
            m_inputNameStorage.emplace_back(inputName.get());
        }

        for (size_t i = 0; i < outputCount; ++i) {
            auto outputName = m_backgroundSession->GetOutputNameAllocated(i, allocator);
            m_outputNameStorage.emplace_back(outputName.get());
        }

        for (const std::string &inputName : m_inputNameStorage) {
            m_inputNames.push_back(inputName.c_str());
        }

        for (const std::string &outputName : m_outputNameStorage) {
            m_outputNames.push_back(outputName.c_str());
        }

        m_inputShape = m_backgroundSession->GetInputTypeInfo(0).GetTensorTypeAndShapeInfo().GetShape();
        m_backgroundSessionReady = true;
        qDebug() << "[ONNX] Модель загружена:" << modelPath
                 << "вход:" << QString::fromStdString(m_inputNameStorage.front())
                 << "выход:" << QString::fromStdString(m_outputNameStorage.front());
        return true;
    } catch (const Ort::Exception &ex) {
        errorMessage = QStringLiteral("Ошибка инициализации ONNX Runtime: %1").arg(QString::fromUtf8(ex.what()));
        return false;
    }
}

bool OnnxWorker::isCancellationRequested() const
{
    return m_cancelRequested.load();
}

bool OnnxWorker::sleepWithCancellationCheck(unsigned long ms)
{
    for (unsigned long elapsed = 0; elapsed < ms; elapsed += 20) {
        if (isCancellationRequested()) {
            return false;
        }
        QThread::msleep(20);
    }

    return !isCancellationRequested();
}

void OnnxWorker::runInference(const QImage &image, int mode)
{
    m_cancelRequested.store(false);

    if (image.isNull()) {
        emit errorOccurred(QStringLiteral("Пустая картинка для обработки"));
        return;
    }

    QElapsedTimer timer;
    timer.start();

    QImage resultImage;
    QString statusText;

    if (mode == ModeBackgroundRemoval || mode == ModeBackgroundColor || mode == ModeBackgroundBlur) {
        QString sessionError;
        if (!ensureBackgroundSession(sessionError)) {
            emit errorOccurred(sessionError);
            return;
        }

        const int modelHeight = m_inputShape.size() >= 3 ? effectiveDimension(m_inputShape[m_inputShape.size() - 2], image.height()) : image.height();
        const int modelWidth = m_inputShape.size() >= 4 ? effectiveDimension(m_inputShape[m_inputShape.size() - 1], image.width()) : image.width();
        QImage resized = image.scaled(modelWidth, modelHeight, Qt::IgnoreAspectRatio, Qt::SmoothTransformation).convertToFormat(QImage::Format_RGB888);

        std::vector<float> inputTensorData(static_cast<size_t>(modelWidth) * static_cast<size_t>(modelHeight) * 3u);
        for (int y = 0; y < modelHeight; ++y) {
            if (isCancellationRequested()) {
                emit inferenceCanceled(QStringLiteral("Удаление фона отменено"));
                return;
            }
            const uchar *scanLine = resized.constScanLine(y);
            for (int x = 0; x < modelWidth; ++x) {
                const int pixelOffset = x * 3;
                const size_t pixelIndex = static_cast<size_t>(y) * static_cast<size_t>(modelWidth) + static_cast<size_t>(x);
                inputTensorData[pixelIndex] = static_cast<float>(scanLine[pixelOffset]) / 255.0f;
                inputTensorData[static_cast<size_t>(modelWidth) * static_cast<size_t>(modelHeight) + pixelIndex] =
                    static_cast<float>(scanLine[pixelOffset + 1]) / 255.0f;
                inputTensorData[2u * static_cast<size_t>(modelWidth) * static_cast<size_t>(modelHeight) + pixelIndex] =
                    static_cast<float>(scanLine[pixelOffset + 2]) / 255.0f;
            }
        }
        qint64 preprocessingTime = timer.elapsed();

        timer.restart();
        try {
            std::vector<int64_t> inputShape = m_inputShape;
            if (inputShape.empty()) {
                inputShape = {1, 3, modelHeight, modelWidth};
            } else {
                if (inputShape.size() >= 1 && inputShape[0] <= 0) inputShape[0] = 1;
                if (inputShape.size() >= 2 && inputShape[1] <= 0) inputShape[1] = 3;
                if (inputShape.size() >= 3 && inputShape[inputShape.size() - 2] <= 0) inputShape[inputShape.size() - 2] = modelHeight;
                if (inputShape.size() >= 4 && inputShape[inputShape.size() - 1] <= 0) inputShape[inputShape.size() - 1] = modelWidth;
            }

            Ort::MemoryInfo memoryInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
            Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
                memoryInfo,
                inputTensorData.data(),
                inputTensorData.size(),
                inputShape.data(),
                inputShape.size()
            );

            auto outputs = m_backgroundSession->Run(
                *m_runOptions,
                m_inputNames.data(),
                &inputTensor,
                1,
                m_outputNames.data(),
                1
            );

            if (isCancellationRequested()) {
                emit inferenceCanceled(QStringLiteral("Удаление фона отменено"));
                return;
            }

            qint64 inferenceTime = timer.elapsed();

            timer.restart();
            const auto outputShape = outputs.front().GetTensorTypeAndShapeInfo().GetShape();
            const float *outputData = outputs.front().GetTensorData<float>();

            int outputHeight = modelHeight;
            int outputWidth = modelWidth;
            int outputChannels = 1;

            if (outputShape.size() >= 2) {
                outputHeight = effectiveDimension(outputShape[outputShape.size() - 2], modelHeight);
                outputWidth = effectiveDimension(outputShape[outputShape.size() - 1], modelWidth);
            }
            if (outputShape.size() >= 3) {
                outputChannels = effectiveDimension(outputShape[outputShape.size() - 3], 1);
            }

            QImage maskSmall(outputWidth, outputHeight, QImage::Format_Grayscale8);
            const size_t channelStride = static_cast<size_t>(outputWidth) * static_cast<size_t>(outputHeight);
            for (int y = 0; y < outputHeight; ++y) {
                if (isCancellationRequested()) {
                    emit inferenceCanceled(QStringLiteral("Удаление фона отменено"));
                    return;
                }
                uchar *maskLine = maskSmall.scanLine(y);
                for (int x = 0; x < outputWidth; ++x) {
                    const size_t pixelIndex = static_cast<size_t>(y) * static_cast<size_t>(outputWidth) + static_cast<size_t>(x);
                    float probability = 0.0f;

                    if (outputChannels <= 1) {
                        probability = toProbability(outputData[pixelIndex]);
                    } else if (outputChannels == 2) {
                        const float backgroundScore = outputData[pixelIndex];
                        const float foregroundScore = outputData[channelStride + pixelIndex];
                        probability = foregroundScore > backgroundScore ? 1.0f : 0.0f;
                    } else {
                        int bestChannel = 0;
                        float bestScore = outputData[pixelIndex];
                        for (int channel = 1; channel < outputChannels; ++channel) {
                            const float score = outputData[static_cast<size_t>(channel) * channelStride + pixelIndex];
                            if (score > bestScore) {
                                bestScore = score;
                                bestChannel = channel;
                            }
                        }
                        probability = bestChannel == 0 ? 0.0f : 1.0f;
                    }

                    maskLine[x] = static_cast<uchar>(qBound(0, qRound(probability * 255.0f), 255));
                }
            }

            const QImage finalMask = maskSmall.scaled(image.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                .convertToFormat(QImage::Format_Grayscale8);
            const QImage sourceImage = image.convertToFormat(QImage::Format_ARGB32);
            resultImage = QImage(image.size(), QImage::Format_ARGB32);
            if (mode == ModeBackgroundRemoval) {
                resultImage.fill(Qt::transparent);
            } else if (mode == ModeBackgroundColor) {
                resultImage.fill(QColor(QStringLiteral("#e8eef5")).rgba());
            } else {
                resultImage = makeBlurredBackdrop(sourceImage);
            }

            for (int y = 0; y < image.height(); ++y) {
                if (isCancellationRequested()) {
                    emit inferenceCanceled(QStringLiteral("Удаление фона отменено"));
                    return;
                }

                const uchar *maskLine = finalMask.constScanLine(y);
                for (int x = 0; x < image.width(); ++x) {
                    const int alpha = maskLine[x];
                    if (alpha <= 0) {
                        continue;
                    }

                    const QRgb foregroundPixel = sourceImage.pixel(x, y);
                    if (mode == ModeBackgroundRemoval) {
                        resultImage.setPixel(x, y, qRgba(qRed(foregroundPixel), qGreen(foregroundPixel), qBlue(foregroundPixel), alpha));
                    } else {
                        const QRgb backgroundPixel = resultImage.pixel(x, y);
                        const int invAlpha = 255 - alpha;
                        const int mixedRed = (qRed(foregroundPixel) * alpha + qRed(backgroundPixel) * invAlpha) / 255;
                        const int mixedGreen = (qGreen(foregroundPixel) * alpha + qGreen(backgroundPixel) * invAlpha) / 255;
                        const int mixedBlue = (qBlue(foregroundPixel) * alpha + qBlue(backgroundPixel) * invAlpha) / 255;
                        resultImage.setPixel(x, y, qRgba(mixedRed, mixedGreen, mixedBlue, 255));
                    }
                }
            }

            const qint64 postprocessingTime = timer.elapsed();
            const QString actionText =
                mode == ModeBackgroundRemoval ? QStringLiteral("Фон удален") :
                mode == ModeBackgroundColor ? QStringLiteral("Фон заменен на цвет") :
                QStringLiteral("Фон размыт");
            statusText = QString("%1 за %2 мс (препроцессинг: %3 мс, инференс: %4 мс, постпроцессинг: %5 мс)")
                             .arg(actionText)
                             .arg(preprocessingTime + inferenceTime + postprocessingTime)
                             .arg(preprocessingTime)
                             .arg(inferenceTime)
                             .arg(postprocessingTime);
        } catch (const Ort::Exception &ex) {
            emit errorOccurred(QStringLiteral("Ошибка инференса ONNX: %1").arg(QString::fromUtf8(ex.what())));
            return;
        }

    } else if (mode == ModeEnhance) {
        // --- 2. ИНСТРУМЕНТ «УЛУЧШЕНИЕ»: АВТОКОРРЕКЦИЯ КАНАЛОВ ---
        qint64 preprocessingTime = timer.elapsed();
        timer.restart();

        resultImage = image.convertToFormat(QImage::Format_RGB888);

        int minL = 255, maxL = 0;
        for (int y = 0; y < resultImage.height(); ++y) {
            if (isCancellationRequested()) {
                emit inferenceCanceled(QStringLiteral("Улучшение изображения отменено"));
                return;
            }
            for (int x = 0; x < resultImage.width(); ++x) {
                int gray = qGray(resultImage.pixel(x, y));
                if (gray < minL) minL = gray;
                if (gray > maxL) maxL = gray;
            }
        }

        if (maxL == minL) maxL++;

        for (int y = 0; y < resultImage.height(); ++y) {
            if (isCancellationRequested()) {
                emit inferenceCanceled(QStringLiteral("Улучшение изображения отменено"));
                return;
            }
            for (int x = 0; x < resultImage.width(); ++x) {
                QRgb p = resultImage.pixel(x, y);
                int r = qBound(0, ((qRed(p) - minL) * 255) / (maxL - minL), 255);
                int g = qBound(0, ((qGreen(p) - minL) * 255) / (maxL - minL), 255);
                int b = qBound(0, ((qBlue(p) - minL) * 255) / (maxL - minL), 255);
                resultImage.setPixel(x, y, qRgb(r, g, b));
            }
        }

        if (!sleepWithCancellationCheck(150)) {
            emit inferenceCanceled(QStringLiteral("Улучшение изображения отменено"));
            return;
        }
        qint64 inferenceTime = timer.elapsed();
        statusText = QString("Контраст и яркость автокорректированы за %1 мс").arg(preprocessingTime + inferenceTime);

    } else if (mode == ModeStyleTransfer) {
        // --- 3. ИНСТРУМЕНТ «СТИЛЬ»: ХУДОЖЕСТВЕННАЯ СТИЛИЗАЦИЯ ---
        qint64 preprocessingTime = timer.elapsed();
        timer.restart();

        resultImage = image.convertToFormat(QImage::Format_RGB888);

        for (int y = 1; y < resultImage.height() - 1; ++y) {
            if (isCancellationRequested()) {
                emit inferenceCanceled(QStringLiteral("Стилизация изображения отменена"));
                return;
            }
            for (int x = 1; x < resultImage.width() - 1; ++x) {
                int grayX = qGray(image.pixel(x+1, y)) - qGray(image.pixel(x-1, y));
                int grayY = qGray(image.pixel(x, y+1)) - qGray(image.pixel(x, y-1));
                int edge = qBound(0, int(std::sqrt(grayX*grayX + grayY*grayY)), 255);

                int inverted = 255 - edge;
                resultImage.setPixel(x, y, qRgb(inverted, qMax(0, inverted - 20), qMax(0, inverted - 50)));
            }
        }

        if (!sleepWithCancellationCheck(200)) {
            emit inferenceCanceled(QStringLiteral("Стилизация изображения отменена"));
            return;
        }
        qint64 inferenceTime = timer.elapsed();
        statusText = QString("Стилизация под графику выполнена за %1 мс").arg(preprocessingTime + inferenceTime);
    }

    emit inferenceFinished(statusText);
    emit imageProcessed(resultImage);
}
