#pragma once

#include <QString>
#include <QFile>
#include <QDataStream>
#include <QByteArray>
#include <vector>

namespace aegis::utils {

/**
 * @brief Minimal ZIP writer using the stored (uncompressed) method.
 *
 * Produces standard ZIP files readable by any unzip tool.
 * No external dependency; uses only Qt file I/O.
 *
 * Limitations: stored method only (no compression), filenames must be ASCII/UTF-8.
 */
class SimpleZipWriter {
public:
    explicit SimpleZipWriter(const QString& zipPath);
    ~SimpleZipWriter();

    /** @brief Add a file to the archive from disk. */
    bool addFile(const QString& sourcePath, const QString& archiveName);

    /** @brief Add data directly as a file entry. */
    bool addData(const QByteArray& data, const QString& archiveName);

    /** @brief Finalize and close the ZIP. Must be called before destruction. */
    bool close();

    /** @brief True if the archive is open and ready. */
    bool isOpen() const { return m_file.isOpen(); }

private:
    struct Entry {
        QString name;
        quint32 crc;
        quint32 size;
        quint64 localHeaderOffset;
    };

    bool writeLocalHeader(const QString& name, quint32 crc, quint32 size);
    bool writeCentralDirectory();
    bool writeEndOfCentralDirectory(quint64 cdOffset, quint32 cdSize, quint16 entryCount);

    static quint32 crc32(const QByteArray& data);

    QFile m_file;
    QDataStream m_stream;
    std::vector<Entry> m_entries;
    bool m_closed{false};
};

} // namespace aegis::utils
