/*
 * MAVLink C runtime round-trip + CRC-rejection test.
 *
 * Loopback pack -> serialize -> parse -> decode round-trip tests for 10
 * representative messages generated from common.xml, plus exactly one
 * CRC-rejection negative test. Mirrors the upstream loopback harness
 * pymavlink/generator/C/test/posix/testmav.c (serialize with
 * mavlink_msg_to_send_buffer, then feed the bytes one-by-one back through
 * mavlink_parse_char). Built only with -DMAVLINK_BUILD_TESTS=ON and registered
 * with CTest by tests/c/CMakeLists.txt; generated headers are produced into the
 * CMake build tree (the source tree is never modified). CRC semantics:
 * mavlink_parse_char reports a bad-CRC frame as MAVLINK_FRAMING_INCOMPLETE (0),
 * never MAVLINK_FRAMING_OK, so the negative test asserts the frame is never
 * accepted as OK. main() returns 0 when all checks pass, 1 otherwise (CTest).
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <float.h>
#include "common/mavlink.h"

#define TEST_SYSID  42
#define TEST_COMPID 11
#define PARSE_CHAN  MAVLINK_COMM_0

static unsigned g_checks = 0;
static unsigned g_failures = 0;

#define CHECK(cond) \
    do { \
        g_checks++; \
        if (!(cond)) { \
            g_failures++; \
            fprintf(stderr, "CHECK FAILED: %s at %s:%d\n", #cond, __FILE__, __LINE__); \
        } \
    } while (0)

static float f_abs(float x) { return x < 0.0f ? -x : x; }
/*
 * Overflow-safe approximate equality for the round-trip float checks.
 * A single-precision "diff <= 1e-5f * (1 + |a| + |b|)" form overflows to +inf
 * for opposite-sign finite extrema (FLT_MAX vs -FLT_MAX): both the difference
 * and the scale saturate, degenerating to "inf <= inf" and wrongly accepting a
 * corrupted extreme. We therefore reject non-finite operands outright and
 * evaluate the difference and relative scale in double precision, whose range
 * trivially holds 2*FLT_MAX (~6.8e38), so no intermediate can overflow and
 * FLT_MAX and -FLT_MAX correctly compare unequal. The small relative-epsilon
 * convention is preserved and no <math.h>/-lm dependency is introduced
 * (f_abs is manual and the float->double promotion is a language feature).
 */
static int float_close(float a, float b) {
    /* Reject non-finite operands: NaN fails self-equality; an infinity has a
     * magnitude strictly greater than the largest finite float. */
    if (a != a || b != b) return 0;
    if (f_abs(a) > FLT_MAX || f_abs(b) > FLT_MAX) return 0;
    /* Evaluate in double so opposite-sign finite extrema cannot overflow. */
    double diff  = (double)a - (double)b;
    if (diff < 0.0) diff = -diff;
    double scale = 1.0 + (double)f_abs(a) + (double)f_abs(b);
    return diff <= 1.0e-5 * scale;
}
#define CHECK_FEQ(a, b) CHECK(float_close((a), (b)))

static int roundtrip(mavlink_message_t *out, const mavlink_message_t *in) {
    uint8_t buf[MAVLINK_MAX_PACKET_LEN];
    mavlink_message_t parsed;
    mavlink_status_t status;
    uint16_t len, i;
    len = mavlink_msg_to_send_buffer(buf, in);
    for (i = 0; i < len; i++) {
        if (mavlink_parse_char(PARSE_CHAN, buf[i], &parsed, &status) == MAVLINK_FRAMING_OK) {
            *out = parsed;
            return 1;
        }
    }
    return 0;
}

static void test_heartbeat(void) {
    mavlink_message_t msg, rx; mavlink_heartbeat_t out;
    mavlink_msg_heartbeat_pack(TEST_SYSID, TEST_COMPID, &msg,
        MAV_TYPE_QUADROTOR, MAV_AUTOPILOT_GENERIC, MAV_MODE_FLAG_SAFETY_ARMED, 0x12345678u, MAV_STATE_ACTIVE);
    CHECK(roundtrip(&rx, &msg));
    mavlink_msg_heartbeat_decode(&rx, &out);
    CHECK(out.type == MAV_TYPE_QUADROTOR);
    CHECK(out.autopilot == MAV_AUTOPILOT_GENERIC);
    CHECK(out.base_mode == MAV_MODE_FLAG_SAFETY_ARMED);
    CHECK(out.custom_mode == 0x12345678u);
    CHECK(out.system_status == MAV_STATE_ACTIVE);
}

static void test_sys_status(void) {
    mavlink_message_t msg, rx; mavlink_sys_status_t out;
    mavlink_msg_sys_status_pack(TEST_SYSID, TEST_COMPID, &msg,
        0x00000001u, 0xFFFFFFFFu, 0x0000FFFFu, 500, 12000, -1, 75,
        10, 20, 30, 40, 50, 60);
    CHECK(roundtrip(&rx, &msg));
    mavlink_msg_sys_status_decode(&rx, &out);
    CHECK(out.onboard_control_sensors_present == 0x00000001u);
    CHECK(out.onboard_control_sensors_enabled == 0xFFFFFFFFu);
    CHECK(out.onboard_control_sensors_health == 0x0000FFFFu);
    CHECK(out.load == 500);
    CHECK(out.voltage_battery == 12000);
    CHECK(out.current_battery == -1);
    CHECK(out.battery_remaining == 75);
    CHECK(out.drop_rate_comm == 10);
    CHECK(out.errors_comm == 20);
    CHECK(out.errors_count1 == 30);
    CHECK(out.errors_count2 == 40);
    CHECK(out.errors_count3 == 50);
    CHECK(out.errors_count4 == 60);
}

static void test_param_value(void) {
    mavlink_message_t msg, rx; mavlink_param_value_t out;
    const char *name = "SYSID_THISMAV";
    mavlink_msg_param_value_pack(TEST_SYSID, TEST_COMPID, &msg,
        name, 3.14159f, MAV_PARAM_TYPE_REAL32, 128, 7);
    CHECK(roundtrip(&rx, &msg));
    mavlink_msg_param_value_decode(&rx, &out);
    CHECK(strncmp(out.param_id, name, 16) == 0);
    CHECK_FEQ(out.param_value, 3.14159f);
    CHECK(out.param_type == MAV_PARAM_TYPE_REAL32);
    CHECK(out.param_count == 128);
    CHECK(out.param_index == 7);
}

static void test_gps_raw_int(void) {
    mavlink_message_t msg, rx; mavlink_gps_raw_int_t out;
    mavlink_msg_gps_raw_int_pack(TEST_SYSID, TEST_COMPID, &msg,
        1234567890ULL, GPS_FIX_TYPE_3D_FIX, INT32_MIN, INT32_MAX, -1000,
        100, 200, 500, 18000, 12, 250, 300, 400, 55, 90, 36000);
    CHECK(roundtrip(&rx, &msg));
    mavlink_msg_gps_raw_int_decode(&rx, &out);
    CHECK(out.time_usec == 1234567890ULL);
    CHECK(out.lat == INT32_MIN);
    CHECK(out.lon == INT32_MAX);
    CHECK(out.alt == -1000);
    CHECK(out.eph == 100);
    CHECK(out.epv == 200);
    CHECK(out.vel == 500);
    CHECK(out.cog == 18000);
    CHECK(out.fix_type == GPS_FIX_TYPE_3D_FIX);
    CHECK(out.satellites_visible == 12);
    CHECK(out.alt_ellipsoid == 250);
    CHECK(out.h_acc == 300);
    CHECK(out.v_acc == 400);
    CHECK(out.vel_acc == 55);
    CHECK(out.hdg_acc == 90);
    CHECK(out.yaw == 36000);
}

static void check_attitude(uint32_t t, float roll) {
    mavlink_message_t msg, rx; mavlink_attitude_t out;
    mavlink_msg_attitude_pack(TEST_SYSID, TEST_COMPID, &msg,
        t, roll, -0.5f, 1.25f, 0.01f, -0.02f, 0.03f);
    CHECK(roundtrip(&rx, &msg));
    mavlink_msg_attitude_decode(&rx, &out);
    CHECK(out.time_boot_ms == t);
    CHECK_FEQ(out.roll, roll);
    CHECK_FEQ(out.pitch, -0.5f);
    CHECK_FEQ(out.yaw, 1.25f);
    CHECK_FEQ(out.rollspeed, 0.01f);
    CHECK_FEQ(out.pitchspeed, -0.02f);
    CHECK_FEQ(out.yawspeed, 0.03f);
}
static void test_attitude(void) {
    check_attitude(123456u, 0.75f);
    check_attitude(0u, -FLT_MAX);
    check_attitude(UINT32_MAX, FLT_MAX);
}

static void test_rc_channels(void) {
    mavlink_message_t msg, rx; mavlink_rc_channels_t out;
    mavlink_msg_rc_channels_pack(TEST_SYSID, TEST_COMPID, &msg,
        7654321u, 18, 1000, 1100, 1200, 1300, 1400, 1500, 1600, 1700, 1800,
        1900, 2000, 900, 950, 1050, 1150, 1250, 1350, 1450, 210);
    CHECK(roundtrip(&rx, &msg));
    mavlink_msg_rc_channels_decode(&rx, &out);
    CHECK(out.time_boot_ms == 7654321u);
    CHECK(out.chancount == 18);
    CHECK(out.chan1_raw == 1000);
    CHECK(out.chan2_raw == 1100);
    CHECK(out.chan3_raw == 1200);
    CHECK(out.chan4_raw == 1300);
    CHECK(out.chan5_raw == 1400);
    CHECK(out.chan6_raw == 1500);
    CHECK(out.chan7_raw == 1600);
    CHECK(out.chan8_raw == 1700);
    CHECK(out.chan9_raw == 1800);
    CHECK(out.chan10_raw == 1900);
    CHECK(out.chan11_raw == 2000);
    CHECK(out.chan12_raw == 900);
    CHECK(out.chan13_raw == 950);
    CHECK(out.chan14_raw == 1050);
    CHECK(out.chan15_raw == 1150);
    CHECK(out.chan16_raw == 1250);
    CHECK(out.chan17_raw == 1350);
    CHECK(out.chan18_raw == 1450);
    CHECK(out.rssi == 210);
}

static void test_mission_item_int(void) {
    mavlink_message_t msg, rx; mavlink_mission_item_int_t out;
    mavlink_msg_mission_item_int_pack(TEST_SYSID, TEST_COMPID, &msg,
        1, 2, 5, MAV_FRAME_GLOBAL, MAV_CMD_NAV_WAYPOINT, 1, 1,
        0.0f, 5.0f, 0.0f, 90.0f, -350000000, 1490000000, 100.5f, MAV_MISSION_TYPE_MISSION);
    CHECK(roundtrip(&rx, &msg));
    mavlink_msg_mission_item_int_decode(&rx, &out);
    CHECK(out.target_system == 1);
    CHECK(out.target_component == 2);
    CHECK(out.seq == 5);
    CHECK(out.frame == MAV_FRAME_GLOBAL);
    CHECK(out.command == MAV_CMD_NAV_WAYPOINT);
    CHECK(out.current == 1);
    CHECK(out.autocontinue == 1);
    CHECK_FEQ(out.param1, 0.0f);
    CHECK_FEQ(out.param2, 5.0f);
    CHECK_FEQ(out.param3, 0.0f);
    CHECK_FEQ(out.param4, 90.0f);
    CHECK(out.x == -350000000);
    CHECK(out.y == 1490000000);
    CHECK_FEQ(out.z, 100.5f);
    CHECK(out.mission_type == MAV_MISSION_TYPE_MISSION);
}

static void test_command_long(void) {
    mavlink_message_t msg, rx; mavlink_command_long_t out;
    mavlink_msg_command_long_pack(TEST_SYSID, TEST_COMPID, &msg,
        1, 1, MAV_CMD_NAV_WAYPOINT, 0, -FLT_MAX, FLT_MAX, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f);
    CHECK(roundtrip(&rx, &msg));
    mavlink_msg_command_long_decode(&rx, &out);
    CHECK(out.target_system == 1);
    CHECK(out.target_component == 1);
    CHECK(out.command == MAV_CMD_NAV_WAYPOINT);
    CHECK(out.confirmation == 0);
    CHECK_FEQ(out.param1, -FLT_MAX);
    CHECK_FEQ(out.param2, FLT_MAX);
    CHECK_FEQ(out.param3, 3.0f);
    CHECK_FEQ(out.param4, 4.0f);
    CHECK_FEQ(out.param5, 5.0f);
    CHECK_FEQ(out.param6, 6.0f);
    CHECK_FEQ(out.param7, 7.0f);
}

static void test_statustext(void) {
    mavlink_message_t msg, rx; mavlink_statustext_t out;
    char text[50]; int i;
    for (i = 0; i < 50; i++) text[i] = (char)('A' + (i % 26));
    mavlink_msg_statustext_pack(TEST_SYSID, TEST_COMPID, &msg,
        MAV_SEVERITY_INFO, text, 0, 0);
    CHECK(roundtrip(&rx, &msg));
    mavlink_msg_statustext_decode(&rx, &out);
    CHECK(out.severity == MAV_SEVERITY_INFO);
    CHECK(memcmp(out.text, text, 50) == 0);
    CHECK(out.id == 0);
    CHECK(out.chunk_seq == 0);
}

static void test_global_position_int(void) {
    mavlink_message_t msg, rx; mavlink_global_position_int_t out;
    mavlink_msg_global_position_int_pack(TEST_SYSID, TEST_COMPID, &msg,
        999u, -350000000, 1490000000, 50000, 1500, -100, 200, -50, 27000);
    CHECK(roundtrip(&rx, &msg));
    mavlink_msg_global_position_int_decode(&rx, &out);
    CHECK(out.time_boot_ms == 999u);
    CHECK(out.lat == -350000000);
    CHECK(out.lon == 1490000000);
    CHECK(out.alt == 50000);
    CHECK(out.relative_alt == 1500);
    CHECK(out.vx == -100);
    CHECK(out.vy == 200);
    CHECK(out.vz == -50);
    CHECK(out.hdg == 27000);
}

static void test_crc_rejection(void) {
    mavlink_message_t msg, parsed; mavlink_status_t status;
    uint8_t buf[MAVLINK_MAX_PACKET_LEN]; uint16_t len, i;
    uint8_t result, saw_ok = 0;
    mavlink_msg_heartbeat_pack(TEST_SYSID, TEST_COMPID, &msg,
        MAV_TYPE_QUADROTOR, MAV_AUTOPILOT_GENERIC, 0, 0, MAV_STATE_ACTIVE);
    len = mavlink_msg_to_send_buffer(buf, &msg);
    buf[len - 1] ^= 0xFF;
    for (i = 0; i < len; i++) {
        result = mavlink_parse_char(PARSE_CHAN, buf[i], &parsed, &status);
        if (result == MAVLINK_FRAMING_OK) saw_ok = 1;
    }
    CHECK(saw_ok == 0);
}

int main(void) {
    test_heartbeat();
    test_sys_status();
    test_param_value();
    test_gps_raw_int();
    test_attitude();
    test_rc_channels();
    test_mission_item_int();
    test_command_long();
    test_statustext();
    test_global_position_int();
    test_crc_rejection();
    printf("ran %u checks, %u failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
