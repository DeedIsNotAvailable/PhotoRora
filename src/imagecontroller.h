#ifndef IMAGECONTROLLER_H
#define IMAGECONTROLLER_H

#include <QObject>
#include <QImage>
#include <QColor>
#include <QString>
#include <QThread>
#include <QVector>
#include "onnxworker.h"

class AiImageProvider;

class ImageController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isProcessing READ isProcessing NOTIFY isProcessingChanged)
    Q_PROPERTY(QString aiResult READ aiResult NOTIFY aiResultChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY historyChanged)
    Q_PROPERTY(QString backgroundColorHex READ backgroundColorHex NOTIFY backgroundColorChanged)

public:
    explicit ImageController(QObject *parent = nullptr);
    virtual ~ImageController();

    Q_INVOKABLE void loadImage(const QString &filePath);
    Q_INVOKABLE void undo();
    Q_INVOKABLE void resetToOriginal();
    Q_INVOKABLE void exportResult();
    Q_INVOKABLE void cancelProcessing();
    // Новые инструменты конвейера обработки для кнопок QML
    Q_INVOKABLE void triggerBackgroundRemoval();
    Q_INVOKABLE void triggerBackgroundColor();
    Q_INVOKABLE void triggerBackgroundBlur();
    Q_INVOKABLE void triggerEnhancement();
    Q_INVOKABLE void triggerStyleTransfer();
    Q_INVOKABLE void setBackgroundColor(const QString &colorValue);

    void setProvider(AiImageProvider *provider);

    bool isProcessing() const { return m_isProcessing; }
    QString aiResult() const { return m_aiResult; }
    bool canUndo() const { return m_history.size() > 1; }
    QString backgroundColorHex() const { return m_backgroundColor.name(); }

signals:
    void imageLoadedSuccessfully();
    void errorOccurred(const QString &message);
    void isProcessingChanged();
    void aiResultChanged();
    void contourReady();
    void historyChanged();
    void backgroundColorChanged();

    // Сигнал отправляет в фоновый поток картинку и целочисленный ID операции
    void startInference(const QImage &image, int mode, const QColor &backgroundColor);

private slots:
    void onInferenceFinished(const QString &result);
    void onInferenceError(const QString &message);
    void onInferenceCanceled(const QString &message);
    void onImageProcessed(const QImage &processedImage);

private:
    void updateUiWithCurrentImage();

    QImage m_originalImage;
    QVector<QImage> m_history;

    bool m_isProcessing = false;
    QString m_aiResult = "";
    QColor m_backgroundColor = QColor(QStringLiteral("#e8eef5"));

    QThread m_workerThread;
    OnnxWorker *m_worker;
    AiImageProvider *m_provider = nullptr;
};

#endif // IMAGECONTROLLER_H
