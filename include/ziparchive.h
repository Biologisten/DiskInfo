#pragma once

#include "miniz.h"

#include <QByteArray>
#include <QString>
#include <QVector>

struct ZipEntry
{
    QString name;
    quint64 uncompressedSize = 0;
    bool isDirectory = false;
    bool isSymlink = false;
    int index = -1;
};

class ZipArchive
{
public:
    ZipArchive();
    ~ZipArchive();

    bool open(const QString &path, QString *error = nullptr);
    void close();

    QVector<ZipEntry> entries();
    bool read(const ZipEntry &entry, QByteArray *data, QString *error = nullptr);

    static bool isSafePath(const QString &path);
    static bool isSymlinkExternalAttr(quint32 externalAttr);

private:
    Q_DISABLE_COPY(ZipArchive)

    mz_zip_archive m_zip;
    bool m_open = false;
};
