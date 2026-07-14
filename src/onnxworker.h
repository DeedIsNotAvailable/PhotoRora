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

    enum StyleVariant {
        StyleCandy,
        StyleMosaic,
        StylePaprika,
        StyleShinkai
    };
    Q_ENUM(StyleVariant)

    explicit OnnxWorker(QObject *parent = nullptr);
    ~OnnxWorker();
    void requestCancel();

public slots:
    // Теперь слот принимает картинку и режим фильтрации
    void runInference(const QImage &image, int mode, const QColor &backgroundColor, int styleVariant);

signals:
    void inferenceFinished(const QString &resultText);
    void errorOccurred(const QString &message);
    void inferenceCanceled(const QString &message);
    void imageProcessed(const QImage &processedImage);

private:
    struct SessionState {
        std::unique_ptr<Ort::Session> session;
        std::unique_ptr<Ort::RunOptions> runOptions;
        std::vector<std::string> inputNameStorage;
        std::vector<std::string> outputNameStorage;
        std::vector<const char *> inputNames;
        std::vector<const char *> outputNames;
        std::vector<int64_t> inputShape;
        bool ready = false;
    };

    bool ensureEnv();
    bool ensureBackgroundSession(QString &errorMessage);
    bool ensureEnhancementSession(QString &errorMessage);
    bool ensureStyleSession(int styleVariant, QString &errorMessage);
    QString resolveModelPath() const;
    QString resolveEnhancementModelPath() const;
    QString resolveStyleModelPath(int styleVariant) const;
    bool isCancellationRequested() const;
    bool sleepWithCancellationCheck(unsigned long ms);
    bool prepareSessionState(SessionState &state, const QString &modelPath, QString &errorMessage);

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
    SessionState m_enhancementSession;
    SessionState m_candyStyleSession;
    SessionState m_mosaicStyleSession;
    SessionState m_paprikaStyleSession;
    SessionState m_shinkaiStyleSession;
};

Q_DECLARE_METATYPE(QImage)

#endif // ONNXWORKER_H
