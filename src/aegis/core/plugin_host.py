"""PluginHost — runtime discovery, loading, and lifecycle management."""

import inspect
import importlib.util
import pkgutil
from pathlib import Path
from typing import Any, Dict, List, Type
from PySide6.QtCore import QObject, Signal

from aegis.core.interfaces import IPlugin, TelemetryBus, VehicleState


class PluginMeta:
    """Lightweight metadata container for discovered plugins."""
    def __init__(self, plugin_id: str, cls: Type[IPlugin], source: Path):
        self.plugin_id = plugin_id
        self.cls = cls
        self.source = source

    def __repr__(self) -> str:
        return f"PluginMeta({self.plugin_id})"


class PluginHost(QObject):
    """
    Manages plugin lifecycle:
    1. Discover IPlugin subclasses in src/aegis/plugins/
    2. Instantiate with bus + state + config
    3. Track active instances and provide teardown
    """

    pluginLoaded = Signal(str)       # plugin_id
    pluginUnloaded = Signal(str)   # plugin_id
    pluginCrashed = Signal(str, Exception)

    def __init__(self, bus: TelemetryBus, state: VehicleState,
                 config: Dict[str, Any], parent: QObject | None = None) -> None:
        super().__init__(parent)
        self._bus = bus
        self._state = state
        self._config = config
        self._active: Dict[str, IPlugin] = {}
        self._registry: List[PluginMeta] = []

    def discover(self, packages: List[str]) -> List[PluginMeta]:
        """Scan packages for IPlugin subclasses."""
        found: List[PluginMeta] = []
        for pkg in packages:
            try:
                import importlib
                package = importlib.import_module(pkg)
                package_path: Path = Path(package.__file__).parent
            except Exception:
                continue

            for _, modname, ispkg in pkgutil.iter_modules([str(package_path)]):
                if ispkg:
                    continue
                try:
                    fullname = f"{pkg}.{modname}"
                    mod = importlib.import_module(fullname)
                    for _, cls in inspect.getmembers(mod, inspect.isclass):
                        if issubclass(cls, IPlugin) and cls is not IPlugin:
                            plugin_id = f"{fullname}.{cls.__name__}"
                            meta = PluginMeta(plugin_id, cls, package_path / f"{modname}.py")
                            found.append(meta)
                except Exception as exc:
                    self.pluginCrashed.emit(f"{fullname}.__init__", exc)
        self._registry = found
        return found

    def load(self, plugin_id: str) -> IPlugin:
        """Instantiate and initialize a plugin by ID."""
        meta = next((m for m in self._registry if m.plugin_id == plugin_id), None)
        if not meta:
            raise ValueError(f"Plugin '{plugin_id}' not found in registry")
        if plugin_id in self._active:
            return self._active[plugin_id]

        instance: IPlugin = meta.cls(parent=self)
        try:
            ok = instance.initialize(self._bus, self._state, self._config)
            if not ok:
                raise RuntimeError(f"Plugin {plugin_id} initialize() returned False")
        except Exception as exc:
            self.pluginCrashed.emit(plugin_id, exc)
            instance.deleteLater()
            raise

        self._active[plugin_id] = instance
        self.pluginLoaded.emit(plugin_id)
        return instance

    def unload(self, plugin_id: str) -> None:
        """Gracefully shut down a plugin."""
        instance = self._active.pop(plugin_id, None)
        if instance:
            try:
                instance.shutdown()
            except Exception as exc:
                self.pluginCrashed.emit(plugin_id, exc)
            finally:
                instance.deleteLater()
            self.pluginUnloaded.emit(plugin_id)

    def unload_all(self) -> None:
        """Teardown every active plugin."""
        for pid in list(self._active.keys()):
            self.unload(pid)

    def active_plugins(self) -> List[IPlugin]:
        return list(self._active.values())
