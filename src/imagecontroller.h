#ifndef IMAGECONTROLLER_H
#define IMAGECONTROLLER_H

#include <QObject>
#include <QImage>
#include <QDebug>
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

#endif
