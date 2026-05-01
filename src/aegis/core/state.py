"""VehicleState — canonical, thread-safe ground truth for the active vehicle."""

from typing import Optional
from dataclasses import dataclass, field
from PySide6.QtCore import QObject, Signal


@dataclass
class AttitudeData:
    roll: float = 0.0
    pitch: float = 0.0
    yaw: float = 0.0
    rollspeed: float = 0.0
    pitchspeed: float = 0.0
    yawspeed: float = 0.0


@dataclass
class PositionData:
    lat: int = 0          # degE7
    lon: int = 0          # degE7
    alt: int = 0          # mm MSL
    relative_alt: int = 0  # mm above home
    vx: int = 0
    vy: int = 0
    vz: int = 0
    hdg: int = 0          # cdeg


@dataclass
class BatteryData:
    battery_remaining: int = -1   # -1 = unknown
    voltage_v: float = 0.0
    current_a: float = 0.0


@dataclass
class SystemStatus:
    system_id: int = 0
    component_id: int = 0
    base_mode: int = 0
    custom_mode: int = 0
    system_status: int = 0
    mav_state: str = "UNKNOWN"
    armed: bool = False


class VehicleState(QObject):
    """
    Single source of truth for all vehicle telemetry.

    Thread-safety: Qt signals serialize updates onto the main thread
    assuming all mutators are called from the bus signal handlers.
    """

    attitudeChanged = Signal(AttitudeData)
    positionChanged = Signal(PositionData)
    batteryChanged = Signal(BatteryData)
    systemStatusChanged = Signal(SystemStatus)
    missionCurrentChanged = Signal(int)

    def __init__(self, parent: QObject | None = None) -> None:
        super().__init__(parent)
        self._attitude = AttitudeData()
        self._position = PositionData()
        self._battery = BatteryData()
        self._system_status = SystemStatus()
        self._mission_current: int = 0

    # ── Properties ──────────────────────────────────────────────────────
    @property
    def attitude(self) -> AttitudeData:
        return self._attitude

    @property
    def position(self) -> PositionData:
        return self._position

    @property
    def battery(self) -> BatteryData:
        return self._battery

    @property
    def system_status(self) -> SystemStatus:
        return self._system_status

    @property
    def mission_current(self) -> int:
        return self._mission_current

    # ── Mutators (call only from TelemetryBus handlers) ────────────────
    def update_attitude(self, data: AttitudeData) -> None:
        self._attitude = data
        self.attitudeChanged.emit(data)

    def update_position(self, data: PositionData) -> None:
        self._position = data
        self.positionChanged.emit(data)

    def update_battery(self, data: BatteryData) -> None:
        self._battery = data
        self.batteryChanged.emit(data)

    def update_system_status(self, data: SystemStatus) -> None:
        self._system_status = data
        self.systemStatusChanged.emit(data)

    def update_mission_current(self, index: int) -> None:
        self._mission_current = index
        self.missionCurrentChanged.emit(index)
