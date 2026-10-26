#include "ziparchive.h"

#include <limits>
#include <QStringList>

ZipArchive::ZipArchive()
{
    mz_zip_zero_struct(&m_zip);
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

    mz_zip_zero_struct(&m_zip);
    const mz_bool ok = mz_zip_reader_init_file(&m_zip, path.toUtf8().constData(), 0);
    if (ok == MZ_FALSE) {
        if (error) {
            *error = QString::fromUtf8(mz_zip_get_error_string(mz_zip_get_last_error(&m_zip)));
        }
        return false;
    }

    m_open = true;
    return true;
}

void ZipArchive::close()
{
    if (m_open) {
        mz_zip_reader_end(&m_zip);
        mz_zip_zero_struct(&m_zip);
        m_open = false;
    }
}

QVector<ZipEntry> ZipArchive::entries()
{
    if (!m_open) {
        return {};
    }

    const mz_uint numFiles = mz_zip_reader_get_num_files(&m_zip);
    QVector<ZipEntry> list;
    list.reserve(static_cast<qsizetype>(numFiles));

    for (mz_uint i = 0; i < numFiles; ++i) {
        mz_zip_archive_file_stat stat;
        if (!mz_zip_reader_file_stat(&m_zip, i, &stat)) {
            continue;
        }

        ZipEntry e;
        e.name = QString::fromUtf8(stat.m_filename);
        e.uncompressedSize = stat.m_uncomp_size;
        e.isDirectory = mz_zip_reader_is_file_a_directory(&m_zip, i) != MZ_FALSE;
        e.isSymlink = isSymlinkExternalAttr(stat.m_external_attr);
        e.index = static_cast<int>(i);
        list.append(e);
    }

    return list;
}

bool ZipArchive::read(const ZipEntry &entry, QByteArray *data, QString *error)
{
    if (!m_open || entry.index < 0) {
        if (error) {
            *error = QStringLiteral("Archive not open or invalid entry index");
        }
        return false;
    }

    if (entry.uncompressedSize > static_cast<quint64>(std::numeric_limits<qsizetype>::max())) {
        if (error) {
            *error = QStringLiteral("Entry too large to allocate in memory");
        }
        return false;
    }

    QByteArray buf;
    buf.resize(static_cast<qsizetype>(entry.uncompressedSize));

    const mz_bool ok = mz_zip_reader_extract_to_mem(
        &m_zip,
        static_cast<mz_uint>(entry.index),
        buf.data(),
        static_cast<size_t>(entry.uncompressedSize),
        0);

    if (ok == MZ_FALSE) {
        if (error) {
            *error = QString::fromUtf8(mz_zip_get_error_string(mz_zip_get_last_error(&m_zip)));
        }
        return false;
    }

    if (data) {
        *data = buf;
    }
    return true;
}

bool ZipArchive::isSafePath(const QString &path)
{
    if (path.isEmpty() || path.contains(QLatin1Char('\0'))) {
        return false;
    }

    QString p = path;
    p.replace(QLatin1Char('\\'), QLatin1Char('/'));

    if (p.startsWith(QLatin1Char('/'))) {
        return false;
    }

    if (p.length() >= 2 && p.at(1) == QLatin1Char(':')) {
        const char ch = p.at(0).toLatin1();
        if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z')) {
            return false;
        }
    }

    const QStringList parts = p.split(QLatin1Char('/'));
    for (const QString &part : parts) {
        if (part == QLatin1String("..")) {
            return false;
        }
    }

    return true;
}

bool ZipArchive::isSymlinkExternalAttr(quint32 externalAttr)
{
    return (externalAttr >> 16 & 0xF000u) == 0xA000u;
}
