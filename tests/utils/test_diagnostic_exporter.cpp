#include <gtest/gtest.h>
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDataStream>
#include <QByteArray>
#include "utils/simple_zip_writer.hpp"
#include "utils/diagnostic_exporter.hpp"

using namespace aegis::utils;

// ── ZIP reading helper for tests ────────────────────────────────────

struct ZipEntryInfo {
    QString name;
    quint32 size;
    quint64 offset;
};

static QList<ZipEntryInfo> listZipEntries(const QString& zipPath) {
    QList<ZipEntryInfo> result;
    QFile f(zipPath);
    if (!f.open(QIODevice::ReadOnly)) return result;

    QByteArray data = f.readAll();
    if (data.size() < 22) return result;

    // Read EOCD from end
    QDataStream ds(data);
    ds.setByteOrder(QDataStream::LittleEndian);
    ds.skipRawData(data.size() - 22);

    quint32 eocdSig, cdSize, cdOffset;
    quint16 diskNum, cdDisk, entryCountDisk, entryCountTotal, commentLen;
    ds >> eocdSig >> diskNum >> cdDisk >> entryCountDisk >> entryCountTotal
      >> cdSize >> cdOffset >> commentLen;

    if (eocdSig != 0x06054b50u) return result;

    // Parse central directory
    ds.device()->seek(cdOffset);
    for (int i = 0; i < entryCountTotal; ++i) {
        quint32 sig, crc, compSize, uncompSize, extAttr, localOffset;
        quint16 verMade, verNeeded, flags, method, modTime, modDate,
                nameLen, extraLen, commentLenCD, diskStart, intAttr;
        ds >> sig;
        if (sig != 0x02014b50u) break;
        ds >> verMade >> verNeeded >> flags >> method >> modTime >> modDate
          >> crc >> compSize >> uncompSize >> nameLen >> extraLen
          >> commentLenCD >> diskStart >> intAttr >> extAttr >> localOffset;

        QByteArray nameBytes(nameLen, '\0');
        ds.readRawData(nameBytes.data(), nameLen);
        ds.skipRawData(extraLen + commentLenCD);

        result.append({QString::fromUtf8(nameBytes), uncompSize, localOffset});
    }
    return result;
}

static QByteArray readZipEntryData(const QString& zipPath, const ZipEntryInfo& info) {
    QFile f(zipPath);
    if (!f.open(QIODevice::ReadOnly)) return QByteArray();

    // Skip local header: 30 bytes fixed + filename length + extra field length
    f.seek(info.offset);
    QByteArray header = f.read(30);
    if (header.size() < 30) return QByteArray();

    QDataStream ds(header);
    ds.setByteOrder(QDataStream::LittleEndian);
    quint32 sig;
    quint16 ver, flags, method, modTime, modDate, nameLen, extraLen;
    ds >> sig >> ver >> flags >> method >> modTime >> modDate;
    ds.skipRawData(12); // skip crc32(4) + compSize(4) + uncompSize(4)
    ds >> nameLen >> extraLen;

    quint64 dataOffset = info.offset + 30 + nameLen + extraLen;
    f.seek(dataOffset);
    return f.read(info.size);
}

// ── Tests ──────────────────────────────────────────────────────────

class SimpleZipWriterTest : public ::testing::Test {
protected:
    void SetUp() override {
        app = new QCoreApplication(argc, argv);
        tempDir = new QTemporaryDir();
    }
    void TearDown() override {
        delete tempDir;
        delete app;
    }
    int argc = 1;
    char* argv[1] = {const_cast<char*>("test")};
    QCoreApplication* app{nullptr};
    QTemporaryDir* tempDir{nullptr};
};

TEST_F(SimpleZipWriterTest, WritesValidZipWithTwoFiles) {
    QString zipPath = tempDir->filePath("test.zip");
    SimpleZipWriter writer(zipPath);
    ASSERT_TRUE(writer.isOpen());

    QByteArray data1 = "Hello, world!";
    QByteArray data2 = "Second file content";
    EXPECT_TRUE(writer.addData(data1, "hello.txt"));
    EXPECT_TRUE(writer.addData(data2, "second.txt"));
    EXPECT_TRUE(writer.close());

    auto entries = listZipEntries(zipPath);
    ASSERT_EQ(entries.size(), 2);
    EXPECT_EQ(entries[0].name, "hello.txt");
    EXPECT_EQ(entries[0].size, static_cast<quint32>(data1.size()));
    EXPECT_EQ(entries[1].name, "second.txt");
    EXPECT_EQ(entries[1].size, static_cast<quint32>(data2.size()));

    // Validate file contents
    EXPECT_EQ(readZipEntryData(zipPath, entries[0]), data1);
    EXPECT_EQ(readZipEntryData(zipPath, entries[1]), data2);
}

TEST_F(SimpleZipWriterTest, AddFileFromDisk) {
    QString sourcePath = tempDir->filePath("source.txt");
    QFile src(sourcePath);
    ASSERT_TRUE(src.open(QIODevice::WriteOnly));
    src.write("Disk source content");
    src.close();

    QString zipPath = tempDir->filePath("disk.zip");
    SimpleZipWriter writer(zipPath);
    ASSERT_TRUE(writer.isOpen());
    EXPECT_TRUE(writer.addFile(sourcePath, "source.txt"));
    EXPECT_TRUE(writer.close());

    auto entries = listZipEntries(zipPath);
    ASSERT_EQ(entries.size(), 1);
    EXPECT_EQ(entries[0].name, "source.txt");
    EXPECT_EQ(readZipEntryData(zipPath, entries[0]), QByteArray("Disk source content"));
}

TEST_F(SimpleZipWriterTest, EmptyZipIsValid) {
    QString zipPath = tempDir->filePath("empty.zip");
    SimpleZipWriter writer(zipPath);
    ASSERT_TRUE(writer.isOpen());
    EXPECT_TRUE(writer.close());

    auto entries = listZipEntries(zipPath);
    EXPECT_EQ(entries.size(), 0);
}

class DiagnosticExporterTest : public ::testing::Test {
protected:
    void SetUp() override {
        app = new QCoreApplication(argc, argv);
        tempDir = new QTemporaryDir();

        logPath = tempDir->filePath("aegis.log");
        QFile log(logPath);
        log.open(QIODevice::WriteOnly);
        log.write("Log line 1\nLog line 2\n");
        log.close();

        QFile log1(tempDir->filePath("aegis.log.1"));
        log1.open(QIODevice::WriteOnly);
        log1.write("Older log content\n");
        log1.close();

        configPath = tempDir->filePath("aegis.json");
        QFile cfg(configPath);
        cfg.open(QIODevice::WriteOnly);
        cfg.write("{\"test\":true}");
        cfg.close();
    }
    void TearDown() override {
        delete tempDir;
        delete app;
    }
    int argc = 1;
    char* argv[1] = {const_cast<char*>("test")};
    QCoreApplication* app{nullptr};
    QTemporaryDir* tempDir{nullptr};
    QString logPath;
    QString configPath;
};

TEST_F(DiagnosticExporterTest, SyncExportCreatesZip) {
    QString outPath = tempDir->filePath("bundle.zip");
    DiagnosticExporter exporter(logPath, 3, configPath);
    QString error;
    EXPECT_TRUE(exporter.exportBundle(outPath, &error)) << error.toStdString();
    EXPECT_TRUE(QFile::exists(outPath));
    EXPECT_GT(QFileInfo(outPath).size(), 0);

    auto entries = listZipEntries(outPath);
    EXPECT_GE(entries.size(), 3);  // manifest + log + config
}

TEST_F(DiagnosticExporterTest, ManifestIsValidJson) {
    QString outPath = tempDir->filePath("bundle.zip");
    DiagnosticExporter exporter(logPath, 3, configPath);
    EXPECT_TRUE(exporter.exportBundle(outPath));

    auto entries = listZipEntries(outPath);
    auto it = std::find_if(entries.begin(), entries.end(),
        [](const ZipEntryInfo& e) { return e.name == "manifest.json"; });
    ASSERT_NE(it, entries.end()) << "manifest.json not found in ZIP";

    QByteArray manifestData = readZipEntryData(outPath, *it);
    ASSERT_FALSE(manifestData.isEmpty());

    QJsonDocument doc = QJsonDocument::fromJson(manifestData);
    ASSERT_FALSE(doc.isNull()) << "Invalid JSON: " << manifestData.toStdString();

    QJsonObject manifest = doc.object();
    EXPECT_EQ(manifest.value("manifestVersion").toInt(), 1);
    EXPECT_EQ(manifest.value("application").toString(), QStringLiteral("AEGIS GCS"));
    EXPECT_FALSE(manifest.value("createdAt").toString().isEmpty());
    EXPECT_TRUE(manifest.contains("gitRevision"));

    QJsonArray files = manifest.value("files").toArray();
    EXPECT_GE(files.size(), 2);
}

TEST_F(DiagnosticExporterTest, MissingLogFileStillSucceeds) {
    QString badLog = tempDir->filePath("nonexistent.log");
    QString outPath = tempDir->filePath("bundle.zip");
    DiagnosticExporter exporter(badLog, 3, configPath);
    QString error;
    EXPECT_TRUE(exporter.exportBundle(outPath, &error)) << error.toStdString();
    EXPECT_TRUE(QFile::exists(outPath));

    auto entries = listZipEntries(outPath);
    EXPECT_GE(entries.size(), 2);  // manifest + config at minimum
}

TEST_F(DiagnosticExporterTest, AsyncExportEmitsSignal) {
    QString outPath = tempDir->filePath("async_bundle.zip");
    DiagnosticExporter exporter(logPath, 3, configPath);

    bool finished = false;
    bool success = false;
    QString receivedPath;
    QString receivedError;

    QObject::connect(&exporter, &DiagnosticExporter::exportFinished,
        [&finished, &success, &receivedPath, &receivedError
        ](bool ok, const QString& path, const QString& err) {
            finished = true;
            success = ok;
            receivedPath = path;
            receivedError = err;
        });

    exporter.exportBundleAsync(outPath);

    QElapsedTimer timer;
    timer.start();
    while (!finished && timer.elapsed() < 5000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }

    EXPECT_TRUE(finished);
    EXPECT_TRUE(success);
    EXPECT_EQ(receivedPath, outPath);
    EXPECT_TRUE(receivedError.isEmpty());
    EXPECT_TRUE(QFile::exists(outPath));
}
