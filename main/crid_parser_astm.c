/**
 * crid_parser_astm.c 鈥?ASTM F3411 鍗忚瑙ｆ瀽妯″潡
 *
 * 涓撻棬澶勭悊 ASTM F3411 鍗忚鐨勬暟鎹В鏋? */
#include <string.h>
#include "esp_log.h"
#include "opendroneid.h"
#include "odid_wifi.h"
#include "crid_parser.h"
#include "crid_json.h"
#include "crid_rx_types.h"

// static const char *TAG = "unused";

#define ASTM_MAGIC              0xF2
#define ASTM_HEADER_LEN         3   /* Magic(1)+Size(1)+Count(1) - payload 不再包含 Counter */
#define ASTM_MSG_SIZE           25
#define ASTM_PACK_MAX_MSGS      ODID_PACK_MAX_MESSAGES


/* ================================================================
 * 内部辅助函数
 * ================================================================ */
static inline uint16_t le16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static inline int32_t le32s(const uint8_t *p) {
    return (int32_t)((uint32_t)p[0] | ((uint32_t)p[1] << 8) |
                     ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24));
}

/* ================================================================
 * ASTM F3411 Packed 格式解析
 * ================================================================ */

/**
 * 瑙ｆ瀽 ASTM F3411 鍗忚鏁版嵁
 */
bool crid_parser_decode_astm(uav_track_t *uav, const uint8_t *data, uint8_t len) {
    if (!data || len < 1) return false;

    /* 策略 2: ASTM F3411 Packed 格式 */
    /* payload 结构: [Magic(0xF2)][Size(1)][Count(1)][Messages...] */
    if (len >= ASTM_HEADER_LEN && data[0] == ASTM_MAGIC) {
        uint8_t msg_count = data[2];
        size_t pack_size  = sizeof(ODID_MessagePack_encoded) -
                            ASTM_MSG_SIZE * (ASTM_PACK_MAX_MSGS - msg_count);
        if (len >= pack_size) {
            int ret = odid_message_process_pack(&uav->uas_data, (uint8_t *)data, len);
            if (ret > 0) {
                return true;
            }
        }
    }

    /* 绛栫暐 4: ASTM 鍗曟秷鎭牸寮?(Fallback) - 鐩存帴浠?data[0] 寮€濮?*/
    {
        ODID_messagetype_t t0 = decodeMessageType(data[0]);
        if (t0 >= ODID_MESSAGETYPE_BASIC_ID && t0 <= ODID_MESSAGETYPE_OPERATOR_ID) {
            if (len >= ASTM_MSG_SIZE) {
                if (decodeOpenDroneID(&uav->uas_data, (uint8_t *)data) == ODID_SUCCESS) {
                    return true;
                }
            }
        }
    }

    return false;
}