#ifndef ONNXWORKER_H
#define ONNXWORKER_H

#include <QObject>
#include <QImage>
#include <QMetaType>
#include <atomic>

class OnnxWorker : public QObject
{
    Q_OBJECT
public:
    enum FilterMode {
        ModeBackgroundRemoval,
        ModeEnhance,
        ModeStyleTransfer
    };
    Q_ENUM(FilterMode)

    explicit OnnxWorker(QObject *parent = nullptr);
    void requestCancel();

public slots:
    // Теперь слот принимает картинку и режим фильтрации
    void runInference(const QImage &image, int mode);

signals:
    void inferenceFinished(const QString &resultText);
    void errorOccurred(const QString &message);
    void inferenceCanceled(const QString &message);
    void imageProcessed(const QImage &processedImage);

private:
    bool isCancellationRequested() const;
    bool sleepWithCancellationCheck(unsigned long ms);

    std::atomic_bool m_cancelRequested{false};
};

Q_DECLARE_METATYPE(QImage)

#endif // ONNXWORKER_H
