#ifndef ONNXWORKER_H
#define ONNXWORKER_H

#include <QObject>
#include <QImage>
#include <QMetaType>

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

public slots:
    // Теперь слот принимает картинку и режим фильтрации
    void runInference(const QImage &image, int mode);

signals:
    void inferenceFinished(const QString &resultText);
    void errorOccurred(const QString &message);
    void imageProcessed(const QImage &processedImage);
};

Q_DECLARE_METATYPE(QImage)

#endif // ONNXWORKER_H
