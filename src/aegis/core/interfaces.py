"""AEGIS Plugin SDK — Abstract Base Classes and contracts."""

from abc import ABC, abstractmethod
from typing import Any, ClassVar, Dict, Optional, Callable
from PySide6.QtCore import QObject
from PySide6.QtWidgets import QWidget


class VehicleCommand:
    """Normalized command envelope sent from plugins → vehicle."""

    def __init__(self, command_id: int, target_system: int, target_component: int,
                 params: tuple[float, float, float, float, float, float, float]):
        self.command_id = command_id
        self.target_system = target_system
        self.target_component = target_component
        self.params = params


class TelemetryBus(QObject):
    """Placeholder for type hints; fully defined in bus.py."""
    def subscribe(self, topic: str, callback: Callable[..., Any]) -> None: ...
    def publish(self, topic: str, data: Any) -> None: ...
    def post_command(self, cmd: VehicleCommand) -> None: ...


class VehicleState(QObject):
    """Placeholder for type hints; fully defined in state.py."""
    pass


class IPlugin(ABC, QObject):
    """Base contract for all AEGIS plugins."""

    name: ClassVar[str] = "Unnamed Plugin"
    version: ClassVar[str] = "0.0.0"
    author: ClassVar[str] = ""
    category: ClassVar[str] = "General"
    permissions: ClassVar[list[str]] = []
    min_api_version: ClassVar[str] = "1.0"

    def __init__(self, parent: Optional[QObject] = None) -> None:
        super().__init__(parent)

    @abstractmethod
    def initialize(self, bus: TelemetryBus, state: VehicleState, config: Dict[str, Any]) -> bool:
        """Called once when the plugin is loaded. Return True on success."""
        ...

    @abstractmethod
    def widget(self) -> QWidget:
        """Return the primary Qt widget for display."""
        ...

    @abstractmethod
    def shutdown(self) -> None:
        """Release resources, unsubscribe, and clean up."""
        ...

    def metadata(self) -> Dict[str, Any]:
        return {
            "id": f"{self.__module__}.{self.__class__.__name__}",
            "name": self.name,
            "version": self.version,
            "author": self.author,
            "category": self.category,
            "permissions": self.permissions,
            "min_api_version": self.min_api_version,
        }
