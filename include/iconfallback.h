#pragma once

#include <QColor>
#include <QIcon>
#include <QString>

// Icon shipped in resources.qrc (dist/icons), tinted to color.
QIcon bundledIcon(const QString &name, const QColor &color);

// The desktop icon theme icon when it can actually be rendered, otherwise the
// bundled icon tinted to color.
QIcon themedIconOrFallback(const QString &name, const QColor &color);
