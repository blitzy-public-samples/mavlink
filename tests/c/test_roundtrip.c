/*
  MAVLink 2 C round-trip test for the generated `common` dialect.
  Pack -> serialize -> parse -> decode for 10 representative messages
  (typical + min/max edges) plus one corrupted-CRC rejection test.
  Plain C, no third-party framework; deterministic values only.
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <float.h>   /* FLT_MAX for float edge values */

#define MAVLINK_USE_CONVENIENCE_FUNCTIONS
#define MAVLINK_USE_MESSAGE_INFO
#define MAVLINK_COMM_NUM_BUFFERS 2

/* make mavlink_message_t as small as the dialect needs (upstream trick) */
#include <version.h>
#define MAVLINK_MAX_PAYLOAD_LEN MAVLINK_MAX_DIALECT_PAYLOAD_SIZE

#include <mavlink_types.h>
static mavlink_system_t mavlink_system = {42, 11,};

#define MAVLINK_ASSERT(x) assert(x)
/* Empty stub: required because MAVLINK_USE_CONVENIENCE_FUNCTIONS is defined.
   The round-trip is driven manually with mavlink_msg_to_send_buffer +
   mavlink_frame_char_buffer (see loopback below), so this stub does not
   need to route bytes. */
static void comm_send_ch(mavlink_channel_t chan, uint8_t c) { (void)chan; (void)c; }

#include <mavlink.h>   /* resolves to common/mavlink.h via -I<bindir>/common */

/* ------------------------------------------------------------------------- *
 * In-file assert-style check macro (no third-party C test framework).       *
 * On failure it prints the failing condition and location, then exit(1).    *
 * ------------------------------------------------------------------------- */
#define CHECK(cond) do { if (!(cond)) { \
    fprintf(stderr, "CHECK failed: %s at %s:%d\n", #cond, __FILE__, __LINE__); \
    exit(1); } } while (0)

/* ------------------------------------------------------------------------- *
 * Loopback helper: feed a serialized buffer byte-by-byte into the parser.   *
 * Returns true if a complete, CRC-valid frame was decoded into `rx`         *
 * (i.e. the parser reported MAVLINK_FRAMING_OK); false otherwise.           *
 *                                                                           *
 * Isolation: this uses mavlink_frame_char_buffer() with CALLER-OWNED local  *
 * parse buffers -- a fresh mavlink_message_t and mavlink_status_t that are   *
 * zeroed on entry -- rather than the channel API (mavlink_parse_char). The   *
 * channel API keeps its parse state in the library's internal static        *
 * per-channel arrays (mavlink_get_channel_status/buffer), so passing a local *
 * mavlink_status_t there would only receive an OUTPUT copy and would NOT     *
 * reset the actual parser between calls. Owning the parse state locally      *
 * means every loopback starts from a pristine parser, so each round-trip is  *
 * truly isolated and parallel-safe (this mirrors the upstream test_issues.c  *
 * loopback pattern).                                                         *
 *                                                                           *
 * Note: mavlink_frame_char_buffer() returns MAVLINK_FRAMING_BAD_CRC (2) on a *
 * bad checksum, so the result is compared explicitly against                 *
 * MAVLINK_FRAMING_OK (1). A plain truthiness test would treat a bad-CRC (2)  *
 * frame as success and defeat the corrupted-CRC negative test.               *
 * ------------------------------------------------------------------------- */
static bool loopback(const uint8_t *buf, uint16_t n, mavlink_message_t *rx) {
    mavlink_message_t parse_buf;                     /* local parse message buffer */
    mavlink_status_t  parse_status;                  /* local parse state          */
    mavlink_status_t  r_status;                      /* per-message output status  */
    memset(&parse_buf, 0, sizeof(parse_buf));        /* pristine parser each call  */
    memset(&parse_status, 0, sizeof(parse_status));
    for (uint16_t i = 0; i < n; i++) {
        if (mavlink_frame_char_buffer(&parse_buf, &parse_status, buf[i], rx, &r_status)
                == MAVLINK_FRAMING_OK) {
            return true;
        }
    }
    return false;
}

/* ------------------------------------------------------------------------- *
 * Per-message round-trip functions (10 representative messages).            *
 * Each: pack -> mavlink_msg_to_send_buffer -> loopback parse -> decode,     *
 * then assert the packed inputs survive the round-trip. The designated      *
 * edge field(s) are always checked; several neighbouring fields are         *
 * checked too for a richer verification.                                    *
 * ------------------------------------------------------------------------- */

/* HEARTBEAT (id 0) -- edge int: custom_mode (uint32). */
static void rt_heartbeat(uint32_t custom_mode) {
    mavlink_message_t msg, rx;
    mavlink_heartbeat_t out;
    uint8_t buf[MAVLINK_MAX_PACKET_LEN];
    mavlink_msg_heartbeat_pack(1, 1, &msg,
        MAV_TYPE_QUADROTOR, MAV_AUTOPILOT_GENERIC, 81, custom_mode, MAV_STATE_ACTIVE);
    uint16_t n = mavlink_msg_to_send_buffer(buf, &msg);
    CHECK(loopback(buf, n, &rx));
    mavlink_msg_heartbeat_decode(&rx, &out);
    CHECK(out.custom_mode == custom_mode);
    CHECK(out.type == MAV_TYPE_QUADROTOR);
    CHECK(out.autopilot == MAV_AUTOPILOT_GENERIC);
    CHECK(out.base_mode == 81);
    CHECK(out.system_status == MAV_STATE_ACTIVE);
}

/* SYS_STATUS (id 1) -- edge int: onboard_control_sensors_present (uint32). */
static void rt_sys_status(uint32_t present) {
    mavlink_message_t msg, rx;
    mavlink_sys_status_t out;
    uint8_t buf[MAVLINK_MAX_PACKET_LEN];
    mavlink_msg_sys_status_pack(1, 1, &msg,
        present, 0x0F0F0F0FUL, 0x00FF00FFUL,     /* present, enabled, health */
        500, 12000, -50, 75,                     /* load, voltage, current, remaining */
        10, 20, 1, 2, 3, 4);                     /* drop_rate, errors_comm, errors_count1..4 */
    uint16_t n = mavlink_msg_to_send_buffer(buf, &msg);
    CHECK(loopback(buf, n, &rx));
    mavlink_msg_sys_status_decode(&rx, &out);
    /* assert every packed field survives the round-trip */
    CHECK(out.onboard_control_sensors_present == present);
    CHECK(out.onboard_control_sensors_enabled == 0x0F0F0F0FUL);
    CHECK(out.onboard_control_sensors_health == 0x00FF00FFUL);
    CHECK(out.load == 500);
    CHECK(out.voltage_battery == 12000);
    CHECK(out.current_battery == -50);
    CHECK(out.battery_remaining == 75);
    CHECK(out.drop_rate_comm == 10);
    CHECK(out.errors_comm == 20);
    CHECK(out.errors_count1 == 1);
    CHECK(out.errors_count2 == 2);
    CHECK(out.errors_count3 == 3);
    CHECK(out.errors_count4 == 4);
}

/* PARAM_VALUE (id 22) -- edge int: param_index (uint16); edge float: param_value. */
static void rt_param_value(uint16_t param_index, float param_value) {
    mavlink_message_t msg, rx;
    mavlink_param_value_t out;
    uint8_t buf[MAVLINK_MAX_PACKET_LEN];
    mavlink_msg_param_value_pack(1, 1, &msg,
        "TESTPARAM", param_value, MAV_PARAM_TYPE_REAL32, 100, param_index);
    uint16_t n = mavlink_msg_to_send_buffer(buf, &msg);
    CHECK(loopback(buf, n, &rx));
    mavlink_msg_param_value_decode(&rx, &out);
    /* param_id is char[16] on the wire, NUL/zero-padded past the 9-char
       literal; compare all 16 bytes against the zero-padded expected value */
    char expected_id[16];
    memset(expected_id, 0, sizeof(expected_id));
    memcpy(expected_id, "TESTPARAM", 9);
    CHECK(memcmp(out.param_id, expected_id, sizeof(expected_id)) == 0);
    CHECK(out.param_value == param_value);
    CHECK(out.param_type == MAV_PARAM_TYPE_REAL32);
    CHECK(out.param_count == 100);
    CHECK(out.param_index == param_index);
}

/* GPS_RAW_INT (id 24) -- edge int: lat (int32). */
static void rt_gps_raw_int(int32_t lat) {
    mavlink_message_t msg, rx;
    mavlink_gps_raw_int_t out;
    uint8_t buf[MAVLINK_MAX_PACKET_LEN];
    mavlink_msg_gps_raw_int_pack(1, 1, &msg,
        1234567890123ULL, 3, lat, -1223456789L, 1000L,   /* time_usec, fix_type, lat, lon, alt */
        50000, 200, 300, 400, 12,                         /* eph, epv, vel, cog, satellites_visible */
        51000L, 100UL, 200UL, 300UL, 400UL, 9000);        /* alt_ellipsoid, h/v/vel/hdg_acc, yaw */
    uint16_t n = mavlink_msg_to_send_buffer(buf, &msg);
    CHECK(loopback(buf, n, &rx));
    mavlink_msg_gps_raw_int_decode(&rx, &out);
    /* assert every packed field survives the round-trip */
    CHECK(out.time_usec == 1234567890123ULL);
    CHECK(out.fix_type == 3);
    CHECK(out.lat == lat);
    CHECK(out.lon == -1223456789L);
    CHECK(out.alt == 1000L);
    CHECK(out.eph == 50000);
    CHECK(out.epv == 200);
    CHECK(out.vel == 300);
    CHECK(out.cog == 400);
    CHECK(out.satellites_visible == 12);
    CHECK(out.alt_ellipsoid == 51000L);
    CHECK(out.h_acc == 100UL);
    CHECK(out.v_acc == 200UL);
    CHECK(out.vel_acc == 300UL);
    CHECK(out.hdg_acc == 400UL);
    CHECK(out.yaw == 9000);
}

/* ATTITUDE (id 30) -- edge int: time_boot_ms (uint32); edge float: roll. */
static void rt_attitude(uint32_t t, float roll) {
    mavlink_message_t msg, rx;
    mavlink_attitude_t out;
    uint8_t buf[MAVLINK_MAX_PACKET_LEN];
    mavlink_msg_attitude_pack(1, 1, &msg, t, roll, 0.1f, 0.2f, 0.3f, 0.4f, 0.5f);
    uint16_t n = mavlink_msg_to_send_buffer(buf, &msg);
    CHECK(loopback(buf, n, &rx));
    mavlink_msg_attitude_decode(&rx, &out);
    /* assert every packed field survives the round-trip */
    CHECK(out.time_boot_ms == t);
    CHECK(out.roll == roll);
    CHECK(out.pitch == 0.1f);
    CHECK(out.yaw == 0.2f);
    CHECK(out.rollspeed == 0.3f);
    CHECK(out.pitchspeed == 0.4f);
    CHECK(out.yawspeed == 0.5f);
}

/* GLOBAL_POSITION_INT (id 33) -- edge int: lat (int32). */
static void rt_global_position_int(int32_t lat) {
    mavlink_message_t msg, rx;
    mavlink_global_position_int_t out;
    uint8_t buf[MAVLINK_MAX_PACKET_LEN];
    mavlink_msg_global_position_int_pack(1, 1, &msg,
        123456UL, lat, 87654321L, 1000L, 500L,   /* time_boot_ms, lat, lon, alt, relative_alt */
        10, 20, 30, 18000);                       /* vx, vy, vz, hdg */
    uint16_t n = mavlink_msg_to_send_buffer(buf, &msg);
    CHECK(loopback(buf, n, &rx));
    mavlink_msg_global_position_int_decode(&rx, &out);
    /* assert every packed field survives the round-trip */
    CHECK(out.time_boot_ms == 123456UL);
    CHECK(out.lat == lat);
    CHECK(out.lon == 87654321L);
    CHECK(out.alt == 1000L);
    CHECK(out.relative_alt == 500L);
    CHECK(out.vx == 10);
    CHECK(out.vy == 20);
    CHECK(out.vz == 30);
    CHECK(out.hdg == 18000);
}

/* RC_CHANNELS (id 65) -- edge int: chan1_raw (uint16). 18 channels + rssi. */
static void rt_rc_channels(uint16_t chan1_raw) {
    mavlink_message_t msg, rx;
    mavlink_rc_channels_t out;
    uint8_t buf[MAVLINK_MAX_PACKET_LEN];
    mavlink_msg_rc_channels_pack(1, 1, &msg,
        999999UL, 18, chan1_raw,
        1002, 1003, 1004, 1005, 1006, 1007, 1008, 1009, 1010,
        1011, 1012, 1013, 1014, 1015, 1016, 1017, 1018,
        250);
    uint16_t n = mavlink_msg_to_send_buffer(buf, &msg);
    CHECK(loopback(buf, n, &rx));
    mavlink_msg_rc_channels_decode(&rx, &out);
    /* assert every packed field survives the round-trip (all 18 channels) */
    CHECK(out.time_boot_ms == 999999UL);
    CHECK(out.chancount == 18);
    CHECK(out.chan1_raw == chan1_raw);
    CHECK(out.chan2_raw == 1002);
    CHECK(out.chan3_raw == 1003);
    CHECK(out.chan4_raw == 1004);
    CHECK(out.chan5_raw == 1005);
    CHECK(out.chan6_raw == 1006);
    CHECK(out.chan7_raw == 1007);
    CHECK(out.chan8_raw == 1008);
    CHECK(out.chan9_raw == 1009);
    CHECK(out.chan10_raw == 1010);
    CHECK(out.chan11_raw == 1011);
    CHECK(out.chan12_raw == 1012);
    CHECK(out.chan13_raw == 1013);
    CHECK(out.chan14_raw == 1014);
    CHECK(out.chan15_raw == 1015);
    CHECK(out.chan16_raw == 1016);
    CHECK(out.chan17_raw == 1017);
    CHECK(out.chan18_raw == 1018);
    CHECK(out.rssi == 250);
}

/* MISSION_ITEM_INT (id 73) -- edge int: x (int32); edge float: z.
   Integer literals are used for frame/command/mission_type to keep the test
   self-contained and free of any non-essential enum dependencies. */
static void rt_mission_item_int(int32_t x, float z) {
    mavlink_message_t msg, rx;
    mavlink_mission_item_int_t out;
    uint8_t buf[MAVLINK_MAX_PACKET_LEN];
    mavlink_msg_mission_item_int_pack(1, 1, &msg,
        1, 2, 7, 3, 16, 1, 1,                 /* target_sys, target_comp, seq, frame, command, current, autocontinue */
        1.1f, 2.2f, 3.3f, 4.4f,               /* param1..param4 */
        x, 555L, z, 0);                       /* x, y, z, mission_type */
    uint16_t n = mavlink_msg_to_send_buffer(buf, &msg);
    CHECK(loopback(buf, n, &rx));
    mavlink_msg_mission_item_int_decode(&rx, &out);
    /* assert every packed field survives the round-trip */
    CHECK(out.target_system == 1);
    CHECK(out.target_component == 2);
    CHECK(out.seq == 7);
    CHECK(out.frame == 3);
    CHECK(out.command == 16);
    CHECK(out.current == 1);
    CHECK(out.autocontinue == 1);
    CHECK(out.param1 == 1.1f);
    CHECK(out.param2 == 2.2f);
    CHECK(out.param3 == 3.3f);
    CHECK(out.param4 == 4.4f);
    CHECK(out.x == x);
    CHECK(out.y == 555L);
    CHECK(out.z == z);
    CHECK(out.mission_type == 0);
}

/* COMMAND_LONG (id 76) -- edge int: command (uint16); edge float: param1. */
static void rt_command_long(uint16_t command, float param1) {
    mavlink_message_t msg, rx;
    mavlink_command_long_t out;
    uint8_t buf[MAVLINK_MAX_PACKET_LEN];
    mavlink_msg_command_long_pack(1, 1, &msg,
        1, 2, command, 0,                          /* target_sys, target_comp, command, confirmation */
        param1, 2.2f, 3.3f, 4.4f, 5.5f, 6.6f, 7.7f); /* param1..param7 */
    uint16_t n = mavlink_msg_to_send_buffer(buf, &msg);
    CHECK(loopback(buf, n, &rx));
    mavlink_msg_command_long_decode(&rx, &out);
    /* assert every packed field survives the round-trip */
    CHECK(out.target_system == 1);
    CHECK(out.target_component == 2);
    CHECK(out.command == command);
    CHECK(out.confirmation == 0);
    CHECK(out.param1 == param1);
    CHECK(out.param2 == 2.2f);
    CHECK(out.param3 == 3.3f);
    CHECK(out.param4 == 4.4f);
    CHECK(out.param5 == 5.5f);
    CHECK(out.param6 == 6.6f);
    CHECK(out.param7 == 7.7f);
}

/* STATUSTEXT (id 253) -- edge int: severity (uint8); string edge: text char[50]
   (max-length, NOT NUL-terminated on the wire). */
static void rt_statustext(uint8_t severity, const char *text) {
    mavlink_message_t msg, rx;
    mavlink_statustext_t out;
    uint8_t buf[MAVLINK_MAX_PACKET_LEN];
    /* id and chunk_seq are STATUSTEXT extension fields; use fixed non-zero
       deterministic literals so they are actually carried on the wire and
       verified after decode instead of being left implicitly zero. */
    const uint16_t id = 4242;
    const uint8_t chunk_seq = 7;
    mavlink_msg_statustext_pack(1, 1, &msg, severity, text, id, chunk_seq);
    uint16_t n = mavlink_msg_to_send_buffer(buf, &msg);
    CHECK(loopback(buf, n, &rx));
    mavlink_msg_statustext_decode(&rx, &out);
    /* assert every packed field survives the round-trip */
    CHECK(out.severity == severity);
    CHECK(memcmp(out.text, text, 50) == 0);   /* text is char[50] on the wire */
    CHECK(out.id == id);
    CHECK(out.chunk_seq == chunk_seq);
}

/* ------------------------------------------------------------------------- *
 * The single negative test: a valid MAVLink 2 frame with one corrupted CRC  *
 * byte must NEVER be reported as a good frame by the parser.                 *
 * Pattern follows the upstream test_issues.c malformed-frame precedent.     *
 * ------------------------------------------------------------------------- */
static void test_corrupt_crc(void) {
    mavlink_message_t msg, rx;
    uint8_t buf[MAVLINK_MAX_PACKET_LEN];
    mavlink_msg_heartbeat_pack(1, 1, &msg,
        MAV_TYPE_QUADROTOR, MAV_AUTOPILOT_GENERIC, 0, 7, MAV_STATE_ACTIVE);
    uint16_t n = mavlink_msg_to_send_buffer(buf, &msg);
    CHECK(buf[0] == 0xFD);          /* MAVLink 2 start byte */
    /* A valid MAVLink 2 frame is always at least MAVLINK_NUM_NON_PAYLOAD_BYTES
       (10-byte header + 2-byte CRC = 12) long. Verify the serialized length
       spans the header and both CRC bytes before indexing the final CRC byte. */
    CHECK(n >= MAVLINK_NUM_NON_PAYLOAD_BYTES);
    buf[n - 1] ^= 0xFF;             /* corrupt the final CRC byte (CRC = last 2 bytes) */
    CHECK(!loopback(buf, n, &rx));  /* parser must NEVER report MAVLINK_FRAMING_OK */
}

/* ------------------------------------------------------------------------- *
 * main: exercise each message three times -- typical values, the low edge   *
 * (integer MIN/0 and float -FLT_MAX), and the high edge (integer MAX and    *
 * float FLT_MAX) -- then run the corrupted-CRC rejection test.              *
 * All values are fixed literals or limit macros: fully deterministic.       *
 * ------------------------------------------------------------------------- */
int main(void) {
    char text50[51];
    memset(text50, 'A', 50);
    text50[50] = '\0';   /* 50 chars, max-length edge (NUL only for local safety) */

    rt_heartbeat(12345);           rt_heartbeat(0);                    rt_heartbeat(UINT32_MAX);
    rt_sys_status(0x1234);         rt_sys_status(0);                   rt_sys_status(UINT32_MAX);
    rt_param_value(5, 3.14f);      rt_param_value(0, -FLT_MAX);        rt_param_value(UINT16_MAX, FLT_MAX);
    rt_gps_raw_int(-350000000);    rt_gps_raw_int(INT32_MIN);          rt_gps_raw_int(INT32_MAX);
    rt_attitude(9000, 0.5f);       rt_attitude(0, -FLT_MAX);           rt_attitude(UINT32_MAX, FLT_MAX);
    rt_global_position_int(-1);    rt_global_position_int(INT32_MIN);  rt_global_position_int(INT32_MAX);
    rt_rc_channels(1500);          rt_rc_channels(0);                  rt_rc_channels(UINT16_MAX);
    rt_mission_item_int(-5, 1.0f); rt_mission_item_int(INT32_MIN, -FLT_MAX); rt_mission_item_int(INT32_MAX, FLT_MAX);
    rt_command_long(400, 1.5f);    rt_command_long(0, -FLT_MAX);       rt_command_long(UINT16_MAX, FLT_MAX);
    rt_statustext(6, text50);      rt_statustext(0, text50);           rt_statustext(UINT8_MAX, text50);

    test_corrupt_crc();

    printf("ALL ROUND-TRIP TESTS PASSED\n");
    return 0;
}
