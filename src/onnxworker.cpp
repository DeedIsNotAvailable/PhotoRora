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

int alignedDimension(int value, int alignment, int minimum)
{
    if (alignment <= 1) {
        return qMax(value, minimum);
    }

    const int clamped = qMax(value, minimum);
    return qMax(minimum, (clamped / alignment) * alignment);
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

QImage tensorToImage(const float *outputData, int width, int height)
{
    QImage image(width, height, QImage::Format_RGB888);
    const size_t planeSize = static_cast<size_t>(width) * static_cast<size_t>(height);

    for (int y = 0; y < height; ++y) {
        uchar *scanLine = image.scanLine(y);
        for (int x = 0; x < width; ++x) {
            const size_t index = static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x);
            scanLine[x * 3] = static_cast<uchar>(qBound(0, qRound(outputData[index]), 255));
            scanLine[x * 3 + 1] = static_cast<uchar>(qBound(0, qRound(outputData[planeSize + index]), 255));
            scanLine[x * 3 + 2] = static_cast<uchar>(qBound(0, qRound(outputData[2u * planeSize + index]), 255));
        }
    }

    return image;
}

QImage tensorToAnimeGanImage(const float *outputData, int width, int height)
{
    QImage image(width, height, QImage::Format_RGB888);

    for (int y = 0; y < height; ++y) {
        uchar *scanLine = image.scanLine(y);
        for (int x = 0; x < width; ++x) {
            const size_t index = (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 3u;
            const int r = qBound(0, qRound((outputData[index] + 1.0f) * 127.5f), 255);
            const int g = qBound(0, qRound((outputData[index + 1] + 1.0f) * 127.5f), 255);
            const int b = qBound(0, qRound((outputData[index + 2] + 1.0f) * 127.5f), 255);
            scanLine[x * 3] = static_cast<uchar>(r);
            scanLine[x * 3 + 1] = static_cast<uchar>(g);
            scanLine[x * 3 + 2] = static_cast<uchar>(b);
        }
    }

    return image;
}

QColor rgbToYcbcr(const QColor &color)
{
    const double r = color.redF();
    const double g = color.greenF();
    const double b = color.blueF();

    const double y = 0.299 * r + 0.587 * g + 0.114 * b;
    const double cb = 0.5 + (-0.168736 * r - 0.331264 * g + 0.5 * b);
    const double cr = 0.5 + (0.5 * r - 0.418688 * g - 0.081312 * b);
    return QColor::fromRgbF(qBound(0.0, y, 1.0), qBound(0.0, cb, 1.0), qBound(0.0, cr, 1.0));
}

QRgb ycbcrToRgb(double y, double cb, double cr)
{
    const double centeredCb = cb - 0.5;
    const double centeredCr = cr - 0.5;
    const int r = qBound(0, qRound((y + 1.402 * centeredCr) * 255.0), 255);
    const int g = qBound(0, qRound((y - 0.344136 * centeredCb - 0.714136 * centeredCr) * 255.0), 255);
    const int b = qBound(0, qRound((y + 1.772 * centeredCb) * 255.0), 255);
    return qRgb(r, g, b);
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

QString OnnxWorker::resolveStyleModelPath(int styleVariant) const
{
    QString fileName = QStringLiteral("candy-9.onnx");
    if (styleVariant == StyleMosaic) {
        fileName = QStringLiteral("mosaic-9.onnx");
    } else if (styleVariant == StylePaprika) {
        fileName = QStringLiteral("AnimeGANv2_Paprika.onnx");
    } else if (styleVariant == StyleShinkai) {
        fileName = QStringLiteral("AnimeGANv2_Shinkai.onnx");
    }
    const QStringList candidates = {
        QStringLiteral("/usr/share/ru.omgtu.PhotoRora/lib/") + fileName,
        QDir::cleanPath(QCoreApplication::applicationDirPath() + QStringLiteral("/../share/ru.omgtu.PhotoRora/lib/") + fileName),
        QDir::cleanPath(QCoreApplication::applicationDirPath() + QStringLiteral("/../../share/ru.omgtu.PhotoRora/lib/") + fileName),
        QDir::cleanPath(QDir::currentPath() + QStringLiteral("/data/") + fileName)
    };

    for (const QString &candidate : candidates) {
        if (QFileInfo::exists(candidate)) {
            return candidate;
        }
    }

    return candidates.first();
}

QString OnnxWorker::resolveEnhancementModelPath() const
{
    const QString fileName = QStringLiteral("super-resolution-10.onnx");
    const QStringList candidates = {
        QStringLiteral("/usr/share/ru.omgtu.PhotoRora/lib/") + fileName,
        QDir::cleanPath(QCoreApplication::applicationDirPath() + QStringLiteral("/../share/ru.omgtu.PhotoRora/lib/") + fileName),
        QDir::cleanPath(QCoreApplication::applicationDirPath() + QStringLiteral("/../../share/ru.omgtu.PhotoRora/lib/") + fileName),
        QDir::cleanPath(QDir::currentPath() + QStringLiteral("/data/") + fileName)
    };

    for (const QString &candidate : candidates) {
        if (QFileInfo::exists(candidate)) {
            return candidate;
        }
    }

    return candidates.first();
}

bool OnnxWorker::ensureEnv()
{
    if (!m_env) {
        m_env = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "PhotoRora");
    }

    if (!m_sessionOptions) {
        m_sessionOptions = std::make_unique<Ort::SessionOptions>();
        m_sessionOptions->SetIntraOpNumThreads(1);
        m_sessionOptions->SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_EXTENDED);
    }

    return true;
}

bool OnnxWorker::prepareSessionState(SessionState &state, const QString &modelPath, QString &errorMessage)
{
    if (state.ready) {
        return true;
    }

    if (!QFileInfo::exists(modelPath)) {
        errorMessage = QStringLiteral("Не найден файл модели: %1").arg(modelPath);
        return false;
    }

    try {
        ensureEnv();

        const std::string modelPathUtf8 = modelPath.toStdString();
        state.session = std::make_unique<Ort::Session>(*m_env, modelPathUtf8.c_str(), *m_sessionOptions);
        state.runOptions = std::make_unique<Ort::RunOptions>();

        Ort::AllocatorWithDefaultOptions allocator;
        const size_t inputCount = state.session->GetInputCount();
        const size_t outputCount = state.session->GetOutputCount();
        if (inputCount == 0 || outputCount == 0) {
            errorMessage = QStringLiteral("Модель не содержит входов или выходов");
            return false;
        }

        state.inputNameStorage.clear();
        state.outputNameStorage.clear();
        state.inputNames.clear();
        state.outputNames.clear();
        state.inputNameStorage.reserve(inputCount);
        state.outputNameStorage.reserve(outputCount);
        state.inputNames.reserve(inputCount);
        state.outputNames.reserve(outputCount);

        for (size_t i = 0; i < inputCount; ++i) {
            auto inputName = state.session->GetInputNameAllocated(i, allocator);
            state.inputNameStorage.emplace_back(inputName.get());
        }

        for (size_t i = 0; i < outputCount; ++i) {
            auto outputName = state.session->GetOutputNameAllocated(i, allocator);
            state.outputNameStorage.emplace_back(outputName.get());
        }

        for (const std::string &inputName : state.inputNameStorage) {
            state.inputNames.push_back(inputName.c_str());
        }

        for (const std::string &outputName : state.outputNameStorage) {
            state.outputNames.push_back(outputName.c_str());
        }

        state.inputShape = state.session->GetInputTypeInfo(0).GetTensorTypeAndShapeInfo().GetShape();
        state.ready = true;
        return true;
    } catch (const Ort::Exception &ex) {
        errorMessage = QStringLiteral("Ошибка инициализации ONNX Runtime: %1").arg(QString::fromUtf8(ex.what()));
        return false;
    }
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
        ensureEnv();

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

bool OnnxWorker::ensureStyleSession(int styleVariant, QString &errorMessage)
{
    SessionState *state = &m_candyStyleSession;
    if (styleVariant == StyleMosaic) {
        state = &m_mosaicStyleSession;
    } else if (styleVariant == StylePaprika) {
        state = &m_paprikaStyleSession;
    } else if (styleVariant == StyleShinkai) {
        state = &m_shinkaiStyleSession;
    }
    return prepareSessionState(*state, resolveStyleModelPath(styleVariant), errorMessage);
}

bool OnnxWorker::ensureEnhancementSession(QString &errorMessage)
{
    return prepareSessionState(m_enhancementSession, resolveEnhancementModelPath(), errorMessage);
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

void OnnxWorker::runInference(const QImage &image, int mode, const QColor &backgroundColor, int styleVariant)
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
                resultImage.fill(backgroundColor.rgba());
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
        QString sessionError;
        if (!ensureEnhancementSession(sessionError)) {
            emit errorOccurred(sessionError);
            return;
        }

        const int modelHeight = m_enhancementSession.inputShape.size() >= 3
            ? effectiveDimension(m_enhancementSession.inputShape[m_enhancementSession.inputShape.size() - 2], 224)
            : 224;
        const int modelWidth = m_enhancementSession.inputShape.size() >= 4
            ? effectiveDimension(m_enhancementSession.inputShape[m_enhancementSession.inputShape.size() - 1], 224)
            : 224;

        const QImage sourceImage = image.convertToFormat(QImage::Format_RGB32);
        const QImage resizedSource = sourceImage.scaled(modelWidth, modelHeight, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

        std::vector<float> inputTensorData(static_cast<size_t>(modelWidth) * static_cast<size_t>(modelHeight));
        QImage chromaSource(modelWidth, modelHeight, QImage::Format_RGB32);
        for (int y = 0; y < modelHeight; ++y) {
            if (isCancellationRequested()) {
                emit inferenceCanceled(QString::fromUtf8("Улучшение изображения отменено"));
                return;
            }

            QRgb *chromaLine = reinterpret_cast<QRgb *>(chromaSource.scanLine(y));
            for (int x = 0; x < modelWidth; ++x) {
                const QColor ycbcr = rgbToYcbcr(QColor::fromRgb(resizedSource.pixel(x, y)));
                const size_t index = static_cast<size_t>(y) * static_cast<size_t>(modelWidth) + static_cast<size_t>(x);
                inputTensorData[index] = static_cast<float>(ycbcr.redF());
                chromaLine[x] = qRgb(
                    qBound(0, qRound(ycbcr.greenF() * 255.0), 255),
                    qBound(0, qRound(ycbcr.blueF() * 255.0), 255),
                    0
                );
            }
        }
        const qint64 preprocessingTime = timer.elapsed();
        timer.restart();

        try {
            std::vector<int64_t> inputShape = m_enhancementSession.inputShape;
            if (inputShape.empty()) {
                inputShape = {1, 1, modelHeight, modelWidth};
            } else {
                if (inputShape.size() >= 1 && inputShape[0] <= 0) inputShape[0] = 1;
                if (inputShape.size() >= 2 && inputShape[1] <= 0) inputShape[1] = 1;
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

            auto outputs = m_enhancementSession.session->Run(
                *m_enhancementSession.runOptions,
                m_enhancementSession.inputNames.data(),
                &inputTensor,
                1,
                m_enhancementSession.outputNames.data(),
                1
            );

            if (isCancellationRequested()) {
                emit inferenceCanceled(QString::fromUtf8("Улучшение изображения отменено"));
                return;
            }

            const qint64 inferenceTime = timer.elapsed();
            timer.restart();

            const auto outputShape = outputs.front().GetTensorTypeAndShapeInfo().GetShape();
            const float *outputData = outputs.front().GetTensorData<float>();
            const int outputHeight = outputShape.size() >= 3
                ? effectiveDimension(outputShape[outputShape.size() - 2], modelHeight * 3)
                : modelHeight * 3;
            const int outputWidth = outputShape.size() >= 4
                ? effectiveDimension(outputShape[outputShape.size() - 1], modelWidth * 3)
                : modelWidth * 3;

            const QImage upscaledChroma = chromaSource.scaled(outputWidth, outputHeight, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            resultImage = QImage(outputWidth, outputHeight, QImage::Format_RGB888);

            for (int y = 0; y < outputHeight; ++y) {
                if (isCancellationRequested()) {
                    emit inferenceCanceled(QString::fromUtf8("Улучшение изображения отменено"));
                    return;
                }

                uchar *resultLine = resultImage.scanLine(y);
                for (int x = 0; x < outputWidth; ++x) {
                    const size_t index = static_cast<size_t>(y) * static_cast<size_t>(outputWidth) + static_cast<size_t>(x);
                    const double luminance = qBound(0.0, static_cast<double>(outputData[index]), 1.0);
                    const QColor chroma = QColor::fromRgb(upscaledChroma.pixel(x, y));
                    const QRgb rgb = ycbcrToRgb(luminance, chroma.redF(), chroma.greenF());
                    resultLine[x * 3] = static_cast<uchar>(qRed(rgb));
                    resultLine[x * 3 + 1] = static_cast<uchar>(qGreen(rgb));
                    resultLine[x * 3 + 2] = static_cast<uchar>(qBlue(rgb));
                }
            }

            const qint64 postprocessingTime = timer.elapsed();
            statusText = QString::fromUtf8("Улучшение детализации выполнено за %1 мс (препроцессинг: %2 мс, инференс: %3 мс, постпроцессинг: %4 мс)")
                             .arg(preprocessingTime + inferenceTime + postprocessingTime)
                             .arg(preprocessingTime)
                             .arg(inferenceTime)
                             .arg(postprocessingTime);
        } catch (const Ort::Exception &ex) {
            emit errorOccurred(QString::fromUtf8("Ошибка инференса ONNX: %1").arg(QString::fromUtf8(ex.what())));
            return;
        }

    } else if (mode == ModeStyleTransfer) {
        QString sessionError;
        if (!ensureStyleSession(styleVariant, sessionError)) {
            emit errorOccurred(sessionError);
            return;
        }

        SessionState *styleSessionPtr = &m_candyStyleSession;
        if (styleVariant == StyleMosaic) {
            styleSessionPtr = &m_mosaicStyleSession;
        } else if (styleVariant == StylePaprika) {
            styleSessionPtr = &m_paprikaStyleSession;
        } else if (styleVariant == StyleShinkai) {
            styleSessionPtr = &m_shinkaiStyleSession;
        }
        SessionState &styleSession = *styleSessionPtr;
        const bool isAnimeGanStyle = (styleVariant == StylePaprika || styleVariant == StyleShinkai);
        int modelHeight = isAnimeGanStyle
            ? (styleSession.inputShape.size() >= 3 ? effectiveDimension(styleSession.inputShape[1], image.height()) : image.height())
            : (styleSession.inputShape.size() >= 3 ? effectiveDimension(styleSession.inputShape[styleSession.inputShape.size() - 2], image.height()) : image.height());
        int modelWidth = isAnimeGanStyle
            ? (styleSession.inputShape.size() >= 4 ? effectiveDimension(styleSession.inputShape[2], image.width()) : image.width())
            : (styleSession.inputShape.size() >= 4 ? effectiveDimension(styleSession.inputShape[styleSession.inputShape.size() - 1], image.width()) : image.width());

        QImage resized;
        if (isAnimeGanStyle) {
            const int maxAnimeGanSide = 512;
            const QSize boundedTarget = image.size().scaled(maxAnimeGanSide, maxAnimeGanSide, Qt::KeepAspectRatio);
            modelWidth = alignedDimension(boundedTarget.width(), 32, 32);
            modelHeight = alignedDimension(boundedTarget.height(), 32, 32);
            resized = image.scaled(modelWidth, modelHeight, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                          .convertToFormat(QImage::Format_RGB888);
        } else {
            resized = image.scaled(modelWidth, modelHeight, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                          .convertToFormat(QImage::Format_RGB888);
        }

        std::vector<float> inputTensorData(static_cast<size_t>(modelWidth) * static_cast<size_t>(modelHeight) * 3u);
        for (int y = 0; y < modelHeight; ++y) {
            if (isCancellationRequested()) {
                emit inferenceCanceled(QStringLiteral("Стилизация изображения отменена"));
                return;
            }

            const uchar *scanLine = resized.constScanLine(y);
            for (int x = 0; x < modelWidth; ++x) {
                const int pixelOffset = x * 3;
                if (isAnimeGanStyle) {
                    const size_t pixelIndex =
                        (static_cast<size_t>(y) * static_cast<size_t>(modelWidth) + static_cast<size_t>(x)) * 3u;
                    inputTensorData[pixelIndex] = static_cast<float>(scanLine[pixelOffset]) / 127.5f - 1.0f;
                    inputTensorData[pixelIndex + 1] = static_cast<float>(scanLine[pixelOffset + 1]) / 127.5f - 1.0f;
                    inputTensorData[pixelIndex + 2] = static_cast<float>(scanLine[pixelOffset + 2]) / 127.5f - 1.0f;
                } else {
                    const size_t pixelIndex = static_cast<size_t>(y) * static_cast<size_t>(modelWidth) + static_cast<size_t>(x);
                    inputTensorData[pixelIndex] = static_cast<float>(scanLine[pixelOffset]);
                    inputTensorData[static_cast<size_t>(modelWidth) * static_cast<size_t>(modelHeight) + pixelIndex] =
                        static_cast<float>(scanLine[pixelOffset + 1]);
                    inputTensorData[2u * static_cast<size_t>(modelWidth) * static_cast<size_t>(modelHeight) + pixelIndex] =
                        static_cast<float>(scanLine[pixelOffset + 2]);
                }
            }
        }
        const qint64 preprocessingTime = timer.elapsed();
        timer.restart();

        try {
            std::vector<int64_t> inputShape = styleSession.inputShape;
            if (inputShape.empty()) {
                inputShape = isAnimeGanStyle
                    ? std::vector<int64_t>{1, modelHeight, modelWidth, 3}
                    : std::vector<int64_t>{1, 3, modelHeight, modelWidth};
            } else {
                if (inputShape.size() >= 1 && inputShape[0] <= 0) {
                    inputShape[0] = 1;
                }

                if (isAnimeGanStyle) {
                    if (inputShape.size() >= 2 && inputShape[1] <= 0) inputShape[1] = modelHeight;
                    if (inputShape.size() >= 3 && inputShape[2] <= 0) inputShape[2] = modelWidth;
                    if (inputShape.size() >= 4 && inputShape[3] <= 0) inputShape[3] = 3;
                } else {
                    if (inputShape.size() >= 2 && inputShape[1] <= 0) inputShape[1] = 3;
                    if (inputShape.size() >= 3 && inputShape[inputShape.size() - 2] <= 0) inputShape[inputShape.size() - 2] = modelHeight;
                    if (inputShape.size() >= 4 && inputShape[inputShape.size() - 1] <= 0) inputShape[inputShape.size() - 1] = modelWidth;
                }
            }

            Ort::MemoryInfo memoryInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
            Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
                memoryInfo,
                inputTensorData.data(),
                inputTensorData.size(),
                inputShape.data(),
                inputShape.size()
            );

            auto outputs = styleSession.session->Run(
                *styleSession.runOptions,
                styleSession.inputNames.data(),
                &inputTensor,
                1,
                styleSession.outputNames.data(),
                1
            );

            if (isCancellationRequested()) {
                emit inferenceCanceled(QStringLiteral("Стилизация изображения отменена"));
                return;
            }

            const qint64 inferenceTime = timer.elapsed();
            timer.restart();

            const auto outputShape = outputs.front().GetTensorTypeAndShapeInfo().GetShape();
            const float *outputData = outputs.front().GetTensorData<float>();
            const int outputHeight = isAnimeGanStyle
                ? (outputShape.size() >= 3 ? effectiveDimension(outputShape[1], modelHeight) : modelHeight)
                : (outputShape.size() >= 3 ? effectiveDimension(outputShape[outputShape.size() - 2], modelHeight) : modelHeight);
            const int outputWidth = isAnimeGanStyle
                ? (outputShape.size() >= 4 ? effectiveDimension(outputShape[2], modelWidth) : modelWidth)
                : (outputShape.size() >= 4 ? effectiveDimension(outputShape[outputShape.size() - 1], modelWidth) : modelWidth);

            resultImage = (isAnimeGanStyle
                           ? tensorToAnimeGanImage(outputData, outputWidth, outputHeight)
                           : tensorToImage(outputData, outputWidth, outputHeight))
                              .scaled(image.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

            const qint64 postprocessingTime = timer.elapsed();
            QString styleName = QStringLiteral("Candy");
            if (styleVariant == StyleMosaic) {
                styleName = QStringLiteral("Mosaic");
            } else if (styleVariant == StylePaprika) {
                styleName = QStringLiteral("Paprika");
            } else if (styleVariant == StyleShinkai) {
                styleName = QStringLiteral("Shinkai");
            }
            statusText = QString("Стилизация %1 выполнена за %2 мс (препроцессинг: %3 мс, инференс: %4 мс, постпроцессинг: %5 мс)")
                             .arg(styleName)
                             .arg(preprocessingTime + inferenceTime + postprocessingTime)
                             .arg(preprocessingTime)
                             .arg(inferenceTime)
                             .arg(postprocessingTime);
        } catch (const Ort::Exception &ex) {
            emit errorOccurred(QStringLiteral("Ошибка инференса ONNX: %1").arg(QString::fromUtf8(ex.what())));
            return;
        }
    }

    emit inferenceFinished(statusText);
    emit imageProcessed(resultImage);
}
