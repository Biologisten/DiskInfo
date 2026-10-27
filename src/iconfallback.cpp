#include "iconfallback.h"

#include <QPainter>
#include <QPixmap>

QIcon bundledIcon(const QString &name, const QColor &color)
{
    QIcon icon(QStringLiteral(":/icons/%1.svg").arg(name));
    const QPixmap source = icon.pixmap(QSize(64, 64));
    if (source.isNull()) {
        return icon;
    }

    QPixmap tinted = source;
    QPainter painter(&tinted);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(tinted.rect(), color);
    painter.end();
    return QIcon(tinted);
}

QIcon themedIconOrFallback(const QString &name, const QColor &color)
{
    const QIcon themeIcon = QIcon::fromTheme(name);
    if (!themeIcon.pixmap(QSize(16, 16)).isNull()) {
        return themeIcon;
    }
    return bundledIcon(name, color);
}
