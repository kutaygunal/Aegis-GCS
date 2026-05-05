#include <gtest/gtest.h>
#include <QCoreApplication>
#include <QThread>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QRegularExpression>
#include "utils/logging.hpp"

using namespace aegis::utils;

class LoggerTest : public ::testing::Test {
protected:
    QTemporaryDir m_tempDir;

    void SetUp() override {
        Logger::instance().setLogFile("");
        Logger::instance().setMinLevel(LogLevel::Debug);
        Logger::instance().setRotation(0, 0);
    }

    void TearDown() override {
        Logger::instance().setLogFile("");
        Logger::instance().setRotation(0, 0);
    }
};

TEST_F(LoggerTest, WritesToFile) {
    QString path = m_tempDir.path() + "/test.log";
    Logger::instance().setLogFile(path);
    Logger::instance().log(LogLevel::Info, "Test", "hello");
    Logger::instance().setLogFile("");

    QFile f(path);
    ASSERT_TRUE(f.open(QIODevice::ReadOnly | QIODevice::Text));
    QByteArray data = f.readAll();
    EXPECT_TRUE(data.contains("hello"));
    EXPECT_TRUE(data.contains("INF"));
    EXPECT_TRUE(data.contains("Test"));
}

TEST_F(LoggerTest, FormatIncludesThreadId) {
    QString path = m_tempDir.path() + "/test.log";
    Logger::instance().setLogFile(path);
    Logger::instance().log(LogLevel::Info, "Fmt", "msg");
    Logger::instance().setLogFile("");

    QFile f(path);
    ASSERT_TRUE(f.open(QIODevice::ReadOnly | QIODevice::Text));
    QString text = QString::fromUtf8(f.readAll());

    // Format: [timestamp] [threadId] {level} component: message
    QRegularExpression re(R"(\[\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\] \[[0-9A-Fa-f]+\] \{INF\} Fmt: msg)");
    EXPECT_TRUE(re.match(text).hasMatch()) << text.toStdString();
}

TEST_F(LoggerTest, RotationCreatesBackup) {
    QString path = m_tempDir.path() + "/rotate.log";
    Logger::instance().setLogFile(path);
    Logger::instance().setRotation(100, 3);

    for (int i = 0; i < 20; ++i) {
        Logger::instance().log(LogLevel::Info, "Rot", QString("line %1 with padding").arg(i));
    }
    Logger::instance().setLogFile("");

    EXPECT_TRUE(QFile::exists(path));
    EXPECT_TRUE(QFile::exists(path + ".1"));
    // With only 20 short lines, we may not reach .2, but at least one backup must exist.
}

TEST_F(LoggerTest, RotationMaxCountRespected) {
    QString path = m_tempDir.path() + "/rotate2.log";
    Logger::instance().setLogFile(path);
    Logger::instance().setRotation(50, 2);

    for (int i = 0; i < 50; ++i) {
        Logger::instance().log(LogLevel::Info, "Rot", QString("entry %1").arg(i));
    }
    Logger::instance().setLogFile("");

    EXPECT_TRUE(QFile::exists(path));
    EXPECT_TRUE(QFile::exists(path + ".1"));
    EXPECT_FALSE(QFile::exists(path + ".2")) << "Should not exceed maxFiles=2";
}

TEST_F(LoggerTest, ThreadSafetyStress) {
    QString path = m_tempDir.path() + "/thread.log";
    Logger::instance().setLogFile(path);
    // Disable rotation so all lines are retained for size verification.
    Logger::instance().setRotation(0, 0);

    auto writer = [](int id) {
        for (int i = 0; i < 100; ++i) {
            Logger::instance().log(LogLevel::Debug, "Thread", QString("t%1-i%2").arg(id).arg(i));
        }
    };

    QList<QThread*> threads;
    for (int i = 0; i < 4; ++i) {
        QThread* t = QThread::create(writer, i);
        t->start();
        threads.append(t);
    }
    for (QThread* t : threads) {
        t->wait();
        delete t;
    }
    Logger::instance().setLogFile("");

    qint64 totalSize = 0;
    QFile mainFile(path);
    if (mainFile.exists()) totalSize += mainFile.size();

    // 4 threads x 100 lines ~ 400 lines, each line ~ 60 bytes => ~24KB
    EXPECT_GT(totalSize, 400 * 20);
}

TEST_F(LoggerTest, ThreadSafetyWithRotation) {
    QString path = m_tempDir.path() + "/thread_rot.log";
    Logger::instance().setLogFile(path);
    Logger::instance().setRotation(500, 3);

    auto writer = [](int id) {
        for (int i = 0; i < 100; ++i) {
            Logger::instance().log(LogLevel::Debug, "Thread", QString("t%1-i%2").arg(id).arg(i));
        }
    };

    QList<QThread*> threads;
    for (int i = 0; i < 4; ++i) {
        QThread* t = QThread::create(writer, i);
        t->start();
        threads.append(t);
    }
    for (QThread* t : threads) {
        t->wait();
        delete t;
    }
    Logger::instance().setLogFile("");

    // Verify rotation limits are respected: at most 3 files total.
    int fileCount = 0;
    if (QFile::exists(path)) ++fileCount;
    for (int i = 1; i < 10; ++i) {
        if (QFile::exists(path + QString(".%1").arg(i))) ++fileCount;
    }
    EXPECT_LE(fileCount, 3);

    // No single file should exceed maxSizeBytes by a huge margin.
    QFile f(path);
    if (f.exists()) {
        EXPECT_LE(f.size(), 500 + 2000); // allow some buffer for last write
    }
}

TEST_F(LoggerTest, LogLevelFiltering) {
    QString path = m_tempDir.path() + "/filter.log";
    Logger::instance().setLogFile(path);
    Logger::instance().setMinLevel(LogLevel::Warning);

    Logger::instance().log(LogLevel::Debug, "F", "debug-msg");
    Logger::instance().log(LogLevel::Info, "F", "info-msg");
    Logger::instance().log(LogLevel::Warning, "F", "warn-msg");
    Logger::instance().log(LogLevel::Error, "F", "error-msg");
    Logger::instance().setLogFile("");

    QFile f(path);
    ASSERT_TRUE(f.open(QIODevice::ReadOnly | QIODevice::Text));
    QByteArray data = f.readAll();
    EXPECT_FALSE(data.contains("debug-msg"));
    EXPECT_FALSE(data.contains("info-msg"));
    EXPECT_TRUE(data.contains("warn-msg"));
    EXPECT_TRUE(data.contains("error-msg"));
}
