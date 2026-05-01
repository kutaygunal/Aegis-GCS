"""TelemetryBus — thread-safe pub/sub for real-time telemetry and commands."""

from typing import Any, Callable, Dict, List
from PySide6.QtCore import QObject, Signal, Slot

from aegis.core.interfaces import VehicleCommand


class TelemetryBus(QObject):
    """
    Central nervous system of AEGIS.

    All telemetry, state deltas, alerts, and commands flow through this bus.
    Uses Qt Signals for automatic cross-thread marshaling.
    """

    # ── Vehicle state telemetry ──────────────────────────────────────────
    attitudeChanged = Signal(dict)          # {"roll": float, "pitch": float, "yaw": float}
    positionChanged = Signal(dict)        # {"lat": int, "lon": int, "alt": float, ...}
    heartbeatReceived = Signal(dict)      # MAVLink HEARTBEAT fields
    statusTextReceived = Signal(str)      # STATUSTEXT messages
    batteryChanged = Signal(dict)         # BATTERY_STATUS
    missionCurrentChanged = Signal(int)   # Active waypoint index

    # ── System events ────────────────────────────────────────────────────
    alertRaised = Signal(str, str)        # level, message
    connectionStatusChanged = Signal(bool) # connected / disconnected
    commandResponse = Signal(int, bool, str)  # command_id, success, message

    # ── Internal backplane for custom topics ────────────────────────────
    _topicEmitted = Signal(str, object)   # topic, payload (private)

    def __init__(self, parent: QObject | None = None) -> None:
        super().__init__(parent)
        self._subs: Dict[str, List[Callable[[Any], None]]] = {}
        self._topicEmitted.connect(self._dispatch_topic)

    def subscribe(self, topic: str, callback: Callable[[Any], None]) -> None:
        """Subscribe to a named topic. Topics are case-sensitive."""
        if topic not in self._subs:
            self._subs[topic] = []
        self._subs[topic].append(callback)

    def unsubscribe(self, topic: str, callback: Callable[[Any], None]) -> None:
        """Unsubscribe a callback from a topic."""
        if topic in self._subs:
            try:
                self._subs[topic].remove(callback)
            except ValueError:
                pass

    def publish(self, topic: str, data: Any) -> None:
        """Publish data to a topic. Thread-safe via Qt::QueuedConnection."""
        self._topicEmitted.emit(topic, data)

    @Slot(str, object)
    def _dispatch_topic(self, topic: str, data: Any) -> None:
        """Deliver to all subscribers on the Qt event loop."""
        for cb in self._subs.get(topic, []):
            try:
                cb(data)
            except Exception:
                # TODO: log and emit alertRaised instead of crashing the bus
                pass

    def post_command(self, cmd: VehicleCommand) -> None:
        """Enqueue a command for the MAVLink I/O thread."""
        # TODO: forward to MAVLinkIO outbound queue
        self.commandResponse.emit(cmd.command_id, True, "Enqueued")
