#ifndef ONNXWORKER_H
#define ONNXWORKER_H

#include <QObject>
#include <QImage>
#include <QColor>
#include <QMetaType>
#include <atomic>
#include <memory>
#include <string>
#include <vector>

namespace Ort {
class Env;
class SessionOptions;
class Session;
class RunOptions;
}

class OnnxWorker : public QObject
{
    Q_OBJECT
public:
    enum FilterMode {
        ModeBackgroundRemoval,
        ModeBackgroundColor,
        ModeBackgroundBlur,
        ModeEnhance,
        ModeStyleTransfer
    };
    Q_ENUM(FilterMode)

    explicit OnnxWorker(QObject *parent = nullptr);
    ~OnnxWorker();
    void requestCancel();

public slots:
    // Теперь слот принимает картинку и режим фильтрации
    void runInference(const QImage &image, int mode, const QColor &backgroundColor);

signals:
    void inferenceFinished(const QString &resultText);
    void errorOccurred(const QString &message);
    void inferenceCanceled(const QString &message);
    void imageProcessed(const QImage &processedImage);

private:
    bool ensureBackgroundSession(QString &errorMessage);
    QString resolveModelPath() const;
    bool isCancellationRequested() const;
    bool sleepWithCancellationCheck(unsigned long ms);

    std::atomic_bool m_cancelRequested{false};
    std::unique_ptr<Ort::Env> m_env;
    std::unique_ptr<Ort::SessionOptions> m_sessionOptions;
    std::unique_ptr<Ort::Session> m_backgroundSession;
    std::unique_ptr<Ort::RunOptions> m_runOptions;
    std::vector<std::string> m_inputNameStorage;
    std::vector<std::string> m_outputNameStorage;
    std::vector<const char *> m_inputNames;
    std::vector<const char *> m_outputNames;
    std::vector<int64_t> m_inputShape;
    bool m_backgroundSessionReady = false;
};

Q_DECLARE_METATYPE(QImage)

#endif // ONNXWORKER_H
