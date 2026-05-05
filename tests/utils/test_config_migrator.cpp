#include <gtest/gtest.h>
#include <QCoreApplication>
#include <QJsonObject>
#include <QJsonDocument>
#include <QFile>
#include <QTemporaryDir>
#include <QDir>
#include "utils/config_migrator.hpp"

using namespace aegis::utils;

class ConfigMigratorTest : public ::testing::Test {
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

    QJsonObject makeV1Config() const {
        QJsonObject config;
        config.insert("configVersion", 1);
        QJsonObject telemetry;
        telemetry.insert("bindAddress", "0.0.0.0");
        telemetry.insert("bindPort", 14550);
        telemetry.insert("heartbeatTimeoutMs", 3000);
        config.insert("telemetry", telemetry);
        QJsonObject logging;
        logging.insert("minLevel", "Debug");
        logging.insert("file", "aegis.log");
        config.insert("logging", logging);
        config.insert("customKey", "preserved");
        return config;
    }

    QJsonObject makeV2Config() const {
        QJsonObject config;
        config.insert("configVersion", 2);
        QJsonObject telemetry;
        telemetry.insert("bindAddress", "0.0.0.0");
        telemetry.insert("bindPort", 14550);
        telemetry.insert("heartbeatTimeoutMs", 3000);
        telemetry.insert("reconnectIntervalMs", 5000);
        config.insert("telemetry", telemetry);
        QJsonObject logging;
        logging.insert("minLevel", "Debug");
        logging.insert("file", "aegis.log");
        logging.insert("enableConsole", true);
        config.insert("logging", logging);
        return config;
    }
};

TEST_F(ConfigMigratorTest, NoMigrationNeeded) {
    ConfigMigrator m = ConfigMigrator::createAegisMigrator();
    QJsonObject v2 = makeV2Config();

    QString error;
    QJsonObject result = m.migrate(v2, 2, &error);

    EXPECT_TRUE(error.isEmpty());
    EXPECT_EQ(result.value("configVersion").toInt(), 2);
    EXPECT_EQ(result.value("telemetry").toObject().value("bindPort").toInt(), 14550);
}

TEST_F(ConfigMigratorTest, SingleStepMigrationV1ToV2) {
    ConfigMigrator m = ConfigMigrator::createAegisMigrator();
    QJsonObject v1 = makeV1Config();

    QString error;
    QJsonObject result = m.migrate(v1, 2, &error);

    EXPECT_TRUE(error.isEmpty()) << error.toStdString();
    EXPECT_EQ(result.value("configVersion").toInt(), 2);

    QJsonObject telemetry = result.value("telemetry").toObject();
    EXPECT_TRUE(telemetry.contains("reconnectIntervalMs"));
    EXPECT_EQ(telemetry.value("reconnectIntervalMs").toInt(), 5000);

    QJsonObject logging = result.value("logging").toObject();
    EXPECT_TRUE(logging.contains("enableConsole"));
    EXPECT_TRUE(logging.value("enableConsole").toBool());
}

TEST_F(ConfigMigratorTest, MultiStepMigrationV1ToV3) {
    ConfigMigrator m = ConfigMigrator::createAegisMigrator();
    QJsonObject v1 = makeV1Config();

    QString error;
    QJsonObject result = m.migrate(v1, 3, &error);

    EXPECT_TRUE(error.isEmpty()) << error.toStdString();
    EXPECT_EQ(result.value("configVersion").toInt(), 3);
    // v1→v2 added keys should still be present
    EXPECT_TRUE(result.value("telemetry").toObject().contains("reconnectIntervalMs"));
    EXPECT_TRUE(result.value("logging").toObject().contains("enableConsole"));
}

TEST_F(ConfigMigratorTest, BackupCreated) {
    QJsonObject v1 = makeV1Config();
    QString basePath = tempDir->filePath("aegis.json");

    QString backupPath;
    bool ok = ConfigMigrator::writeBackup(v1, basePath, &backupPath);

    EXPECT_TRUE(ok);
    EXPECT_FALSE(backupPath.isEmpty());
    EXPECT_TRUE(QFile::exists(backupPath));

    QFile file(backupPath);
    ASSERT_TRUE(file.open(QIODevice::ReadOnly));
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    EXPECT_EQ(doc.object().value("configVersion").toInt(), 1);
    EXPECT_EQ(doc.object().value("customKey").toString(), QStringLiteral("preserved"));
}

TEST_F(ConfigMigratorTest, UnknownKeysPreserved) {
    ConfigMigrator m = ConfigMigrator::createAegisMigrator();
    QJsonObject v1 = makeV1Config();
    v1.insert("unknownPlugin", QJsonObject{{"key", 123}});

    QString error;
    QJsonObject result = m.migrate(v1, 2, &error);

    EXPECT_TRUE(error.isEmpty());
    EXPECT_TRUE(result.contains("customKey"));
    EXPECT_EQ(result.value("customKey").toString(), QStringLiteral("preserved"));
    EXPECT_TRUE(result.contains("unknownPlugin"));
}

TEST_F(ConfigMigratorTest, MissingMigrationStepFails) {
    ConfigMigrator m = ConfigMigrator::createAegisMigrator();
    QJsonObject v1 = makeV1Config();

    QString error;
    // Target v4 but only v1→v2 and v2→v3 are registered
    QJsonObject result = m.migrate(v1, 4, &error);

    EXPECT_FALSE(error.isEmpty());
    EXPECT_TRUE(error.contains("Missing migration step"));
    // Should return original config unchanged on failure
    EXPECT_EQ(result.value("configVersion").toInt(), 1);
}

TEST_F(ConfigMigratorTest, MigrationDoesNotTouchExistingValues) {
    ConfigMigrator m = ConfigMigrator::createAegisMigrator();
    QJsonObject v1 = makeV1Config();
    QJsonObject telemetry = v1.value("telemetry").toObject();
    telemetry.insert("bindPort", 9999);
    v1.insert("telemetry", telemetry);

    QString error;
    QJsonObject result = m.migrate(v1, 2, &error);

    EXPECT_TRUE(error.isEmpty());
    EXPECT_EQ(result.value("telemetry").toObject().value("bindPort").toInt(), 9999);
    // New key still added
    EXPECT_EQ(result.value("telemetry").toObject().value("reconnectIntervalMs").toInt(), 5000);
}

TEST_F(ConfigMigratorTest, V0TreatedAsV1) {
    ConfigMigrator m = ConfigMigrator::createAegisMigrator();
    QJsonObject v0;
    // No configVersion key → defaults to 1
    v0.insert("telemetry", QJsonObject{{"bindPort", 14550}});

    QString error;
    QJsonObject result = m.migrate(v0, 2, &error);

    EXPECT_TRUE(error.isEmpty());
    EXPECT_EQ(result.value("configVersion").toInt(), 2);
    EXPECT_TRUE(result.value("telemetry").toObject().contains("reconnectIntervalMs"));
}

TEST_F(ConfigMigratorTest, AlreadyPresentKeysNotOverwritten) {
    ConfigMigrator m = ConfigMigrator::createAegisMigrator();
    QJsonObject v1 = makeV1Config();
    QJsonObject telemetry = v1.value("telemetry").toObject();
    telemetry.insert("reconnectIntervalMs", 9999); // already present
    v1.insert("telemetry", telemetry);

    QString error;
    QJsonObject result = m.migrate(v1, 2, &error);

    EXPECT_TRUE(error.isEmpty());
    // Should preserve existing value, not overwrite with default
    EXPECT_EQ(result.value("telemetry").toObject().value("reconnectIntervalMs").toInt(), 9999);
}
