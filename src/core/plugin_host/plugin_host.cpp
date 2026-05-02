#include "plugin_host.hpp"
#include "../bus/telemetry_bus.hpp"
#include "../state/vehicle_state.hpp"
#include <QDebug>
#include <QCoreApplication>

namespace aegis::core {

PluginHost::PluginHost(TelemetryBus* bus, VehicleState* state,
                       const QVariantMap& config, QObject* parent)
    : QObject(parent), m_bus(bus), m_state(state), m_config(config) {}

PluginHost::~PluginHost() {
    unloadAll();
}

QList<PluginMeta> PluginHost::discover(const QStringList& searchPaths) {
    QList<PluginMeta> results;
    for (const QString& path : searchPaths) {
        QDir dir(path);
        if (!dir.exists()) continue;

        const QStringList filters = {
#ifdef Q_OS_WIN
            "*.dll"
#else
            "*.so"
#endif
        };
        for (const QFileInfo& fi : dir.entryInfoList(filters, QDir::Files)) {
            QPluginLoader* loader = new QPluginLoader(fi.absoluteFilePath());
            const QJsonObject meta = loader->metaData().value("MetaData").toObject();

            PluginMeta pm;
            pm.pluginId = meta.value("id").toString();
            if (pm.pluginId.isEmpty()) continue;  // not an Aegis plugin

            pm.name        = meta.value("name").toString();
            pm.version     = meta.value("version").toString();
            pm.author      = meta.value("author").toString();
            pm.category    = meta.value("category").toString();
            pm.libraryPath = fi.absoluteFilePath();
            pm.permissions = meta.value("permissions").toVariant().toStringList();

            m_registry.insert(pm.pluginId, pm);
            results.append(pm);
            loader->deleteLater();
        }
    }
    return results;
}

IPlugin* PluginHost::load(const QString& pluginId) {
    if (m_active.contains(pluginId)) {
        return m_active.value(pluginId).instance;
    }
    if (!m_registry.contains(pluginId)) {
        emit pluginCrashed(pluginId, "Plugin not found in registry");
        return nullptr;
    }

    const PluginMeta& meta = m_registry.value(pluginId);
    QPluginLoader* loader = new QPluginLoader(meta.libraryPath);
    if (!loader->load()) {
        emit pluginCrashed(pluginId, loader->errorString());
        delete loader;
        return nullptr;
    }

    QObject* obj = loader->instance();
    if (!obj) {
        emit pluginCrashed(pluginId, "QPluginLoader returned null instance");
        delete loader;
        return nullptr;
    }

    IPlugin* plugin = qobject_cast<IPlugin*>(obj);
    if (!plugin) {
        emit pluginCrashed(pluginId, "Library does not implement IPlugin");
        loader->unload();
        delete loader;
        return nullptr;
    }

    try {
        bool ok = plugin->initialize(m_bus, m_state, m_config);
        if (!ok) {
            emit pluginCrashed(pluginId, "initialize() returned false");
            loader->unload();
            delete loader;
            return nullptr;
        }
    } catch (const std::exception& e) {
        emit pluginCrashed(pluginId, QString::fromUtf8(e.what()));
        loader->unload();
        delete loader;
        return nullptr;
    }

    LoadedPlugin lp;
    lp.loader = loader;
    lp.instance = plugin;
    m_active.insert(pluginId, lp);

    emit pluginLoaded(pluginId);
    qDebug() << "[PluginHost] Loaded:" << meta.displayName();
    return plugin;
}

void PluginHost::unload(const QString& pluginId) {
    auto it = m_active.find(pluginId);
    if (it == m_active.end()) return;

    if (it.value().instance) {
        try {
            it.value().instance->shutdown();
        } catch (const std::exception& e) {
            qWarning() << "[PluginHost] Exception during shutdown of"
                     << pluginId << ":" << e.what();
        }
    }
    if (it.value().loader) {
        it.value().loader->unload();
        delete it.value().loader;
    }
    m_active.erase(it);

    emit pluginUnloaded(pluginId);
    qDebug() << "[PluginHost] Unloaded:" << pluginId;
}

void PluginHost::unloadAll() {
    const QStringList ids = m_active.keys();
    for (const QString& id : ids) {
        unload(id);
    }
}

QList<IPlugin*> PluginHost::activePlugins() const {
    QList<IPlugin*> list;
    for (const auto& lp : m_active) {
        list.append(lp.instance);
    }
    return list;
}

bool PluginHost::isLoaded(const QString& pluginId) const {
    return m_active.contains(pluginId);
}

} // namespace aegis::core
