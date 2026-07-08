#ifndef IMAGECONTROLLER_H
#define IMAGECONTROLLER_H

#include <QObject>
#include <QImage>   // Обязательно для QImage
#include <QDebug>   // Обязательно для QDebug
#include <QString>

class ImageController : public QObject
{
    Q_OBJECT
public:
    explicit ImageController(QObject *parent = nullptr);

    Q_INVOKABLE void loadImage(const QString &filePath);

signals:
    void imageLoadedSuccessfully();
    void errorOccurred(const QString &message);

private:
    QImage m_currentImage;
};

#endif // IMAGECONTROLLER_H
