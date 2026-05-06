// Standalone corpus generator — compile and run once, then commit output.
// Not part of the regular build; run manually when corpus format changes.
#include <mavlink.h>
#include <QFile>
#include <QCoreApplication>
#include <QDateTime>

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    QFile file("tests/data/replay/corpus.tlog");
    if (!file.open(QIODevice::WriteOnly)) {
        qFatal("Failed to open corpus.tlog for writing");
    }

    auto writeMsg = [&file](mavlink_message_t& msg) {
        uint8_t buf[MAVLINK_MAX_PACKET_LEN];
        uint16_t len = mavlink_msg_to_send_buffer(buf, &msg);
        file.write(reinterpret_cast<const char*>(buf), len);
    };

    // 1. HEARTBEAT — armed, ACTIVE
    {
        mavlink_message_t msg{};
        mavlink_msg_heartbeat_pack(1, 1, &msg,
            MAV_TYPE_QUADROTOR,
            MAV_AUTOPILOT_PX4,
            MAV_MODE_FLAG_MANUAL_INPUT_ENABLED | MAV_MODE_FLAG_SAFETY_ARMED,
            0,
            MAV_STATE_ACTIVE);
        writeMsg(msg);
    }

    // 2. ATTITUDE
    {
        mavlink_message_t msg{};
        mavlink_msg_attitude_pack(1, 1, &msg,
            1000,       // time_boot_ms
            0.1f,       // roll
            0.2f,       // pitch
            0.3f,       // yaw
            0.01f,      // rollspeed
            0.02f,      // pitchspeed
            0.03f);     // yawspeed
        writeMsg(msg);
    }

    // 3. GLOBAL_POSITION_INT
    {
        mavlink_message_t msg{};
        mavlink_msg_global_position_int_pack(1, 1, &msg,
            2000,           // time_boot_ms
            377000000,      // lat (degE7)
            -1220000000,    // lon (degE7)
            10000,          // alt (mm)
            5000,           // relative_alt (mm)
            0, 0, 0,        // vx, vy, vz (cm/s)
            45);            // hdg (cdeg)
        writeMsg(msg);
    }

    // 4. BATTERY_STATUS
    {
        mavlink_message_t msg{};
        uint16_t voltages[10] = {1520, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
                                 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};
        uint16_t voltages_ext[4] = {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};
        mavlink_msg_battery_status_pack(1, 1, &msg,
            0,                          // id
            MAV_BATTERY_FUNCTION_ALL,
            MAV_BATTERY_TYPE_LIPO,
            850,                        // temperature (cdegC)
            voltages,
            15200,                      // current_battery (cA)
            85,                         // current_consumed (mAh)
            1500,                       // energy_consumed (hJ)
            85,                         // battery_remaining (%)
            0,                          // time_remaining
            0,                          // charge_state
            voltages_ext,
            0,                          // mode
            0);                         // fault_bitmask
        writeMsg(msg);
    }

    // 5. MISSION_CURRENT
    {
        mavlink_message_t msg{};
        mavlink_msg_mission_current_pack(1, 1, &msg,
            3,      // seq
            10,     // total
            0,      // mission_state
            0,      // mission_mode
            0,      // mission_id
            0,      // fence_id
            0);     // rally_points_id
        writeMsg(msg);
    }

    // 6. STATUSTEXT
    {
        mavlink_message_t msg{};
        const char* text = "Low battery warning";
        mavlink_msg_statustext_pack(1, 1, &msg,
            MAV_SEVERITY_WARNING,
            text,
            0,          // id
            0);         // chunk_seq
        writeMsg(msg);
    }

    // 7. HEARTBEAT — disarmed, STANDBY
    {
        mavlink_message_t msg{};
        mavlink_msg_heartbeat_pack(1, 1, &msg,
            MAV_TYPE_QUADROTOR,
            MAV_AUTOPILOT_PX4,
            MAV_MODE_FLAG_MANUAL_INPUT_ENABLED,  // no SAFETY_ARMED
            0,
            MAV_STATE_STANDBY);
        writeMsg(msg);
    }

    file.close();
    return 0;
}
