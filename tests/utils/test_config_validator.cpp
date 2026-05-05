#include <gtest/gtest.h>
#include <QCoreApplication>
#include <QVariantMap>
#include <QVariantList>
#include <QStringList>
#include "utils/config_validator.hpp"

using namespace aegis::utils;

class ConfigValidatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        app = new QCoreApplication(argc, argv);
    }
    void TearDown() override {
        delete app;
    }
    int argc = 1;
    char* argv[1] = {const_cast<char*>("test")};
    QCoreApplication* app{nullptr};
};

TEST_F(ConfigValidatorTest, ValidConfigAcceptedNoWarnings) {
    ConfigValidator v = ConfigValidator::createAegisValidator();

    QVariantMap raw;
    raw.insert("configVersion", 1);
    raw.insert("pluginPaths", QStringList{"./plugins"});
    raw.insert("autostartPlugins", QStringList{
        "aegis.plugins.telemetry_hud",
        "aegis.plugins.alert_console"
    });
    QVariantMap telemetry;
    telemetry.insert("bindAddress", QStringLiteral("127.0.0.1"));
    telemetry.insert("bindPort", 14551);
    telemetry.insert("heartbeatTimeoutMs", 5000);
    raw.insert("telemetry", telemetry);

    QVariantMap logging;
    logging.insert("minLevel", QStringLiteral("Info"));
    logging.insert("file", QStringLiteral("app.log"));
    logging.insert("maxFiles", 3);
    logging.insert("maxSizeBytes", 2048);
    raw.insert("logging", logging);

    QVariantMap dummy;
    dummy.insert("enabled", false);
    dummy.insert("intervalMs", 200);
    raw.insert("dummyTelemetry", dummy);

    QVariantMap plugins;
    QVariantMap alertConsole;
    alertConsole.insert("maxItems", 300);
    alertConsole.insert("showTimestamps", false);
    plugins.insert("aegis.plugins.alert_console", alertConsole);
    raw.insert("plugins", plugins);

    QStringList warnings;
    QVariantMap result = v.validate(raw, &warnings);

    EXPECT_TRUE(warnings.isEmpty()) << warnings.join("; ").toStdString();

    EXPECT_EQ(result.value("configVersion").toInt(), 1);
    EXPECT_EQ(result.value("telemetry").toMap().value("bindPort").toInt(), 14551);
    EXPECT_EQ(result.value("logging").toMap().value("minLevel").toString(), QStringLiteral("Info"));
    EXPECT_EQ(result.value("dummyTelemetry").toMap().value("enabled").toBool(), false);
    EXPECT_EQ(result.value("plugins").toMap()
              .value("aegis.plugins.alert_console").toMap()
              .value("maxItems").toInt(), 300);
}

TEST_F(ConfigValidatorTest, MissingKeyUsesDefault) {
    ConfigValidator v = ConfigValidator::createAegisValidator();

    QVariantMap raw;
    // telemetry.bindPort is missing (optional key)
    QVariantMap telemetry;
    telemetry.insert("bindAddress", QStringLiteral("0.0.0.0"));
    raw.insert("telemetry", telemetry);

    QStringList warnings;
    QVariantMap result = v.validate(raw, &warnings);

    EXPECT_EQ(result.value("telemetry").toMap().value("bindPort").toInt(), 14550);
    // Optional missing keys do not produce warnings
    EXPECT_TRUE(warnings.isEmpty());
}

TEST_F(ConfigValidatorTest, StringCoercedToInt) {
    ConfigValidator v = ConfigValidator::createAegisValidator();

    QVariantMap raw;
    QVariantMap telemetry;
    telemetry.insert("bindPort", QStringLiteral("14599"));
    raw.insert("telemetry", telemetry);

    QStringList warnings;
    QVariantMap result = v.validate(raw, &warnings);

    EXPECT_EQ(result.value("telemetry").toMap().value("bindPort").toInt(), 14599);
    EXPECT_TRUE(warnings.isEmpty());
}

TEST_F(ConfigValidatorTest, WrongTypeUnrecoverableUsesDefault) {
    ConfigValidator v = ConfigValidator::createAegisValidator();

    QVariantMap raw;
    QVariantMap telemetry;
    telemetry.insert("bindPort", QVariantMap{}); // object where int expected
    raw.insert("telemetry", telemetry);

    QStringList warnings;
    QVariantMap result = v.validate(raw, &warnings);

    EXPECT_EQ(result.value("telemetry").toMap().value("bindPort").toInt(), 14550);
    EXPECT_TRUE(std::any_of(warnings.begin(), warnings.end(),
        [](const QString& w) { return w.contains("telemetry.bindPort"); }));
}

TEST_F(ConfigValidatorTest, OutOfRangeClamped) {
    ConfigValidator v = ConfigValidator::createAegisValidator();

    QVariantMap raw;
    QVariantMap logging;
    logging.insert("maxFiles", -1);
    logging.insert("maxSizeBytes", 500); // below min 1024
    raw.insert("logging", logging);

    QStringList warnings;
    QVariantMap result = v.validate(raw, &warnings);

    EXPECT_EQ(result.value("logging").toMap().value("maxFiles").toInt(), 1);
    EXPECT_EQ(result.value("logging").toMap().value("maxSizeBytes").toInt(), 1024);
    EXPECT_EQ(warnings.size(), 2);
}

TEST_F(ConfigValidatorTest, UnknownKeyWarnedAndPreserved) {
    ConfigValidator v = ConfigValidator::createAegisValidator();

    QVariantMap raw;
    raw.insert("unknownKey", QStringLiteral("value"));
    QVariantMap nested;
    nested.insert("foo", 42);
    raw.insert("anotherUnknown", nested);

    QStringList warnings;
    QVariantMap result = v.validate(raw, &warnings);

    EXPECT_TRUE(result.contains("unknownKey"));
    EXPECT_EQ(result.value("unknownKey").toString(), QStringLiteral("value"));
    EXPECT_TRUE(result.contains("anotherUnknown"));
    EXPECT_TRUE(std::any_of(warnings.begin(), warnings.end(),
        [](const QString& w) { return w.contains("unknownKey"); }));
}

TEST_F(ConfigValidatorTest, NestedPluginConfigValid) {
    ConfigValidator v = ConfigValidator::createAegisValidator();

    QVariantMap raw;
    QVariantMap plugins;
    QVariantMap alertConsole;
    alertConsole.insert("maxItems", 500);
    plugins.insert("aegis.plugins.alert_console", alertConsole);
    raw.insert("plugins", plugins);

    QStringList warnings;
    QVariantMap result = v.validate(raw, &warnings);

    EXPECT_TRUE(warnings.isEmpty());
    EXPECT_EQ(result.value("plugins").toMap()
              .value("aegis.plugins.alert_console").toMap()
              .value("maxItems").toInt(), 500);
}

TEST_F(ConfigValidatorTest, NestedPluginConfigInvalidTypeUsesDefault) {
    ConfigValidator v = ConfigValidator::createAegisValidator();

    QVariantMap raw;
    QVariantMap plugins;
    QVariantMap alertConsole;
    alertConsole.insert("maxItems", QStringLiteral("abc")); // string where int expected
    plugins.insert("aegis.plugins.alert_console", alertConsole);
    raw.insert("plugins", plugins);

    QStringList warnings;
    QVariantMap result = v.validate(raw, &warnings);

    EXPECT_EQ(result.value("plugins").toMap()
              .value("aegis.plugins.alert_console").toMap()
              .value("maxItems").toInt(), 200);
    EXPECT_TRUE(std::any_of(warnings.begin(), warnings.end(),
        [](const QString& w) { return w.contains("plugins.aegis.plugins.alert_console.maxItems"); }));
}

TEST_F(ConfigValidatorTest, EmptyConfigAllDefaults) {
    ConfigValidator v = ConfigValidator::createAegisValidator();

    QStringList warnings;
    QVariantMap result = v.validate(QVariantMap{}, &warnings);

    EXPECT_EQ(result.value("configVersion").toInt(), 1);
    EXPECT_EQ(result.value("telemetry").toMap().value("bindPort").toInt(), 14550);
    EXPECT_EQ(result.value("logging").toMap().value("minLevel").toString(), QStringLiteral("Debug"));
    EXPECT_EQ(result.value("dummyTelemetry").toMap().value("enabled").toBool(), true);
    EXPECT_EQ(result.value("plugins").toMap()
              .value("aegis.plugins.alert_console").toMap()
              .value("maxItems").toInt(), 200);
}

TEST_F(ConfigValidatorTest, InvalidMinLevelUsesDefault) {
    ConfigValidator v = ConfigValidator::createAegisValidator();

    QVariantMap raw;
    QVariantMap logging;
    logging.insert("minLevel", QStringLiteral("Trace")); // not in allowed set
    raw.insert("logging", logging);

    QStringList warnings;
    QVariantMap result = v.validate(raw, &warnings);

    EXPECT_EQ(result.value("logging").toMap().value("minLevel").toString(), QStringLiteral("Debug"));
    EXPECT_TRUE(std::any_of(warnings.begin(), warnings.end(),
        [](const QString& w) { return w.contains("logging.minLevel"); }));
}

TEST_F(ConfigValidatorTest, UnknownPluginPreserved) {
    ConfigValidator v = ConfigValidator::createAegisValidator();

    QVariantMap raw;
    QVariantMap plugins;
    QVariantMap unknownPlugin;
    unknownPlugin.insert("customKey", 123);
    plugins.insert("aegis.plugins.unknown", unknownPlugin);
    raw.insert("plugins", plugins);

    QStringList warnings;
    QVariantMap result = v.validate(raw, &warnings);

    EXPECT_TRUE(result.value("plugins").toMap().contains("aegis.plugins.unknown"));
    EXPECT_TRUE(std::any_of(warnings.begin(), warnings.end(),
        [](const QString& w) { return w.contains("plugins.aegis.plugins.unknown"); }));
}
