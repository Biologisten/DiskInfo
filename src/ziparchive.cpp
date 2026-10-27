#include "ziparchive.h"

#include <QFile>
#include <QRegularExpression>
#include <QStringList>

#include <climits>

ZipArchive::ZipArchive()
{
    mz_zip_zero_struct(&m_zip);
    m_open = false;
}

ZipArchive::~ZipArchive()
{
    close();
}

bool ZipArchive::open(const QString &path, QString *error)
{
    if (m_open) {
        close();
    }
    const QByteArray encoded = QFile::encodeName(path);
    m_open = mz_zip_reader_init_file(&m_zip, encoded.constData(), 0) != MZ_FALSE;
    if (!m_open && error) {
        *error = QString::fromUtf8(mz_zip_get_error_string(m_zip.m_last_error));
    }
    return m_open;
}

void ZipArchive::close()
{
    if (m_open) {
        mz_zip_reader_end(&m_zip);
        m_open = false;
        mz_zip_zero_struct(&m_zip);
    }
}

QVector<ZipEntry> ZipArchive::entries()
{
    QVector<ZipEntry> result;
    if (!m_open) {
        return result;
    }

    const mz_uint count = mz_zip_reader_get_num_files(&m_zip);
    result.reserve(int(count));
    for (mz_uint i = 0; i < count; ++i) {
        mz_zip_archive_file_stat stat;
        if (!mz_zip_reader_file_stat(&m_zip, i, &stat)) {
            continue;
        }

        const mz_uint nameSize = mz_zip_reader_get_filename(&m_zip, i, nullptr, 0);
        QByteArray name(int(nameSize), '\0');
        mz_zip_reader_get_filename(&m_zip, i, name.data(), nameSize);
        name.chop(1);

        ZipEntry entry;
        entry.name = QString::fromUtf8(name);
        entry.uncompressedSize = stat.m_uncomp_size;
        entry.isDirectory = stat.m_is_directory != MZ_FALSE;
        entry.isSymlink = isSymlinkExternalAttr(stat.m_external_attr);
        entry.index = int(i);
        result.append(entry);
    }
    return result;
}

bool ZipArchive::read(const ZipEntry &entry, QByteArray *data, QString *error)
{
    if (!m_open || !data) {
        return false;
    }
    if (entry.index < 0) {
        if (error) {
            *error = QStringLiteral("Invalid archive entry index.");
        }
        return false;
    }
    if (entry.uncompressedSize == 0) {
        *data = QByteArray();
        return true;
    }
    if (entry.uncompressedSize > quint64(INT_MAX)) {
        if (error) {
            *error = QStringLiteral("Archive entry is too large to read.");
        }
        return false;
    }

    QByteArray buffer(int(entry.uncompressedSize), '\0');
    if (!mz_zip_reader_extract_to_mem(&m_zip, mz_uint(entry.index),
                                      buffer.data(), size_t(buffer.size()), 0)) {
        if (error) {
            *error = QString::fromUtf8(mz_zip_get_error_string(m_zip.m_last_error));
        }
        return false;
    }
    *data = buffer;
    return true;
}

bool ZipArchive::isSafePath(const QString &path)
{
    if (path.isEmpty() || path.contains(QChar(0))) {
        return false;
    }

    QString normalized = path;
    normalized.replace(QLatin1Char('\\'), QLatin1Char('/'));
    if (normalized.startsWith(QLatin1Char('/'))) {
        return false;
    }

    static const QRegularExpression driveLetter(QStringLiteral("^[A-Za-z]:"));
    if (driveLetter.match(normalized).hasMatch()) {
        return false;
    }

    const QStringList parts = normalized.split(QLatin1Char('/'));
    for (const QString &part : parts) {
        if (part == QLatin1String("..")) {
            return false;
        }
    }
    return true;
}

bool ZipArchive::isSymlinkExternalAttr(quint32 externalAttr)
{
    return ((externalAttr >> 16) & 0xF000u) == 0xA000u;
}
