#ifndef AIIMAGEPROVIDER_H
#define AIIMAGEPROVIDER_H

#include <QQuickImageProvider>
#include <QImage>

class AiImageProvider : public QQuickImageProvider
{
public:
    AiImageProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}

    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override {
        Q_UNUSED(id); Q_UNUSED(requestedSize);
        if (size) *size = m_resultImage.size();
        return m_resultImage;
    }

    void setImage(const QImage &img) { m_resultImage = img; }

private:
    QImage m_resultImage;
};

#endif // AIIMAGEPROVIDER_H
