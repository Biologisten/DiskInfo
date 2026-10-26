#include "ziparchive.h"
#include <QFileInfo>
#include <QDir>

ZipArchive::ZipArchive() { mz_zip_zero_struct(&m_zip); m_open = false; }
ZipArchive::~ZipArchive() { close(); }

bool ZipArchive::open(const QString &path, QString *error)
{
    if (m_open) close();
    m_open = (mz_zip_reader_init_file(&m_zip, path.toUtf8().constData(), 0) != MZ_FALSE);
    if (!m_open && error) *error = QString::fromUtf8(mz_zip_get_error_string(m_zip.m_last_error));
    return m_open;
}

void ZipArchive::close()
{
    if (m_open) { mz_zip_reader_end(&m_zip); m_open = false; mz_zip_zero_struct(&m_zip); }
}

QVector<ZipEntry> ZipArchive::entries()
{
    QVector<ZipEntry> res;
    if (!m_open) return res;
    mz_uint32 num = mz_zip_reader_get_num_files(&m_zip);
    for (mz_uint32 i = 0; i < num; ++i) {
        mz_zip_archive_file_stat stat;
        if (mz_zip_reader_get_filename(&m_zip, i, nullptr, 0, nullptr) == 0) continue;
        // Simplified: use file_stat
        if (!mz_zip_reader_file_stat(&m_zip, i, &stat)) continue;
        ZipEntry e;
        char fname[1024];
        mz_zip_reader_get_filename(&m_zip, i, fname, sizeof(fname), nullptr);
        e.name = QString::fromUtf8(fname);
        e.uncompressedSize = stat.m_uncompr_size;
        e.isDirectory = mz_zip_reader_is_file_a_directory(&m_zip, i);
        e.isSymlink = (stat.m_external_attr >> 16 & 0xF000u) == 0xA000u;
        e.index = (int)i;
        res.append(e);
    }
    return res;
}

bool ZipArchive::read(const ZipEntry &entry, QByteArray *data, QString *error)
{
    if (!m_open) return false;
    size_t size = 0;
    void *p = mz_zip_reader_extract_to_heap(&m_zip, (mz_uint32)entry.index, &size, 0);
    if (!p) { if (error) *error = QString::fromUtf8(mz_zip_get_error_string(m_zip.m_last_error)); return false; }
    data->append((const char*)p, (int)size);
    mz_free(p);
    return true;
}

bool ZipArchive::isSafePath(const QString &path)
{
    if (path.isEmpty()) return false;
    QString s = path;
    s.replace('\\', '/');
    if (s.startsWith('/')) return false;
    if (QRegularExpression("^[A-Za-z]:").match(s).hasMatch()) return false;
    QStringList parts = s.split('/');
    for (const QString &p : parts) {
        if (p == "..") return false;
        if (p.contains('\0')) return false;
    }
    return true;
}

bool ZipArchive::isSymlinkExternalAttr(quint32 externalAttr)
{
    return (externalAttr >> 16 & 0xF000u) == 0xA000u;
}
