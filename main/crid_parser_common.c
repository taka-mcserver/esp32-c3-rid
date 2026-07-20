/**
 * crid_parser_common.c 鈥?鍗忚瑙ｆ瀽閫氱敤妯″潡
 *
 * 鍖呭惈鎵€鏈夊崗璁叡鐢ㄧ殑閫氱敤鍔熻兘
 */

#include <string.h>
#include "esp_log.h"
#include "opendroneid.h"
#include "odid_wifi.h"
#include "crid_parser.h"
#include "crid_json.h"
#include "crid_rx_types.h"

// static const char *TAG = "unused";

/*
 * Debug 寮€鍏筹細璁句负 1 鏃讹紝鍦ㄨВ鏋愬墠鎵撳嵃鍘熷鏁版嵁鍗佸叚杩涘埗杞偍
 * ================================================================ */
#ifndef PARSER_DEBUG_HEX_DUMP
#define PARSER_DEBUG_HEX_DUMP   0
#endif

/* ================================================================
 * Debug 杈呭姪锛氬崄鍏繘鍒惰浆鍌? * ================================================================ */
#if PARSER_DEBUG_HEX_DUMP
static void hex_dump(const char *tag, const char *prefix, const uint8_t *data, uint8_t len) {
    /* 鏍煎紡: <prefix> [len] AA BB CC DD ... (姣忚鏈€澶?16 瀛楄妭) */
    char line[128];
    int pos = 0;
    pos += snprintf(line + pos, sizeof(line) - pos, "%s [%u] ", prefix, len);
    for (int i = 0; i < len; i++) {
        pos += snprintf(line + pos, sizeof(line) - pos, "%02X ", data[i]);
        if ((i + 1) % 16 == 0 && i + 1 < len) {
            ESP_LOGI(tag, "%s", line);
            pos = snprintf(line, sizeof(line), "       ");
        }
    }
    if (pos > 0) {
        ESP_LOGI(tag, "%s", line);
    }
}
#endif

/* ================================================================
 * 涓昏В鏋愬叆鍙ｏ細绛栫暐鍒嗗彂 (鍚槻璇垽鍋ュ悍妫€鏌?
 * ================================================================ */
rid_protocol_t crid_parser_decode(uav_track_t *uav, const uint8_t *data, uint8_t len) {
    if (!data || len < 1) return RID_PROTOCOL_UNKNOWN;
    #if PARSER_DEBUG_HEX_DUMP
    hex_dump(TAG,"Beacon Payload HEX", data, len);
    #endif

    /* 瑙ｆ瀽骞跺垽鏂崗璁被鍨?*/

    // 灏濊瘯瑙ｆ瀽 GB 46750 鍗忚
    if (crid_parser_decode_gb46750(uav, data, len)) {
        return RID_PROTOCOL_GB46750;
    }

    // 灏濊瘯瑙ｆ瀽 GB 42590 鍗忚
    if (crid_parser_decode_gb42590(uav, data, len)) {
        return RID_PROTOCOL_GB42590;
    }

    // 灏濊瘯瑙ｆ瀽 ASTM F3411 鍗忚
    if (crid_parser_decode_astm(uav, data, len)) {
        return RID_PROTOCOL_ASTM_F3411;
    }

    /* 瑙ｆ瀽澶辫触缁熻 */
    static uint32_t s_fail_count = 0;
    if ((++s_fail_count & 0x1F) == 0) {
        json_decode_fail(data[0], (len > 1 ? data[1] : 0), len);
    }
    return RID_PROTOCOL_UNKNOWN;
}

/* ================================================================
 * 鍒嗗眰鏁版嵁鎻愬彇 (GB 46750 浼樺厛锛屽惁鍒欒蛋 ASTM 鏍囧噯瀛楁)
 * ================================================================ */
void crid_parser_extract_layered(uav_track_t *uav) {
    if (!uav) return;

    /* --- GB 46750-2025 映射 --- */
    if (uav->protocol == RID_PROTOCOL_GB46750 && uav->gb46750.valid) {
        gb46750_data_t *gb = &uav->gb46750;

        if (gb->has_unique_id) {
            uav->basic_id.valid = true;
            uav->basic_id.id_type = ODID_IDTYPE_SERIAL_NUMBER;
            uav->basic_id.ua_type = gb->has_ua_category ? gb->ua_category : ODID_UATYPE_HELICOPTER_OR_MULTIROTOR;

            // [鍏抽敭淇] 瀛楃涓插畨鍏ㄥ噣鍖栵細鍓旈櫎 \x01 绛変笉鍙鎺у埗瀛楃锛岀‘淇?JSON 杈撳嚭骞插噣
            char *dst = uav->basic_id.uas_id;
            const char *src = gb->unique_id;
            int i = 0;
            while (*src && i < (int)sizeof(uav->basic_id.uas_id) - 1) {
                if (*src >= 32 && *src <= 126) { // 仅保留可打印 ASCII
                    *dst++ = *src;
                    i++;
                }
                src++;
            }
            *dst = '\0';
        }

        EXTRACT_IF(gb->has_uav_location,      uav->location.latitude,  gb->uav_latitude);
        EXTRACT_IF(gb->has_uav_location,      uav->location.longitude, gb->uav_longitude);
        EXTRACT_IF(gb->has_geo_altitude,      uav->location.altitude_geo, gb->geo_altitude);
        EXTRACT_IF(gb->has_baro_altitude,     uav->location.altitude_baro, gb->baro_altitude);
        EXTRACT_IF(gb->has_relative_height,   uav->location.height, gb->relative_height);
        EXTRACT_IF(gb->has_relative_height,   uav->location.height_ref, ODID_HEIGHT_REF_OVER_TAKEOFF);
        EXTRACT_IF(gb->has_ground_speed,      uav->location.speed_horizontal, gb->ground_speed);
        EXTRACT_IF(gb->has_vertical_speed,    uav->location.speed_vertical, gb->vertical_speed);
        EXTRACT_IF(gb->has_track_angle,       uav->location.direction, gb->track_angle);
        EXTRACT_IF(gb->has_operation_status,  uav->location.status, gb->operation_status);
        EXTRACT_IF(gb->has_h_accuracy,        uav->location.h_accuracy, gb->h_accuracy);
        EXTRACT_IF(gb->has_v_accuracy,        uav->location.v_accuracy, gb->v_accuracy);
        EXTRACT_IF(gb->has_speed_accuracy,    uav->location.speed_accuracy, gb->speed_accuracy);
        EXTRACT_IF(gb->has_ts_accuracy,       uav->location.ts_accuracy, gb->ts_accuracy);
        EXTRACT_IF(gb->has_timestamp,         uav->location.timestamp, gb->timestamp_ms / 1000.0f);

        if (gb->has_uav_location || gb->has_geo_altitude || gb->has_baro_altitude ||
            gb->has_ground_speed || gb->has_track_angle || gb->has_operation_status) {
            uav->location.valid = true;
        }

        /* 閬ユ帶绔欎俊鎭?*/
        if (gb->has_rcs_loc_type) uav->system.operator_location_type = gb->rcs_loc_type;
        EXTRACT_IF(gb->has_rcs_location, uav->system.operator_latitude,  gb->rcs_latitude);
        EXTRACT_IF(gb->has_rcs_location, uav->system.operator_longitude, gb->rcs_longitude);
        EXTRACT_IF(gb->has_rcs_altitude, uav->system.operator_altitude_geo, gb->rcs_altitude);
        if (gb->has_rcs_location || gb->has_rcs_altitude || gb->has_rcs_loc_type) uav->system.valid = true;
        EXTRACT_IF(gb->has_operation_category, uav->system.classification_type, gb->operation_category);
        return;
    }

    /* --- ASTM / GB 42590 鏍囧噯瀛楁鏄犲皠 --- */
    #define MAP_ODID_FIELD(dst, src, valid_cond) \
        do { if (valid_cond) { (dst) = (uint8_t)(src); } } while(0)

    /* Basic ID */
    uav->basic_id.valid = false;
    if (uav->uas_data.BasicIDValid[0]) {
        const ODID_BasicID_data *b = &uav->uas_data.BasicID[0];
        uav->basic_id.valid   = true;
        uav->basic_id.id_type = (uint8_t)b->IDType;
        uav->basic_id.ua_type = (uint8_t)b->UAType;
        strncpy(uav->basic_id.uas_id, b->UASID, sizeof(uav->basic_id.uas_id) - 1);
        uav->basic_id.uas_id[sizeof(uav->basic_id.uas_id) - 1] = '\0';
    }

    /* Location */
    uav->location.valid = uav->uas_data.LocationValid;
    if (uav->location.valid) {
        const ODID_Location_data *l = &uav->uas_data.Location;
        uav->location.latitude        = l->Latitude;
        uav->location.longitude       = l->Longitude;
        uav->location.altitude_baro   = l->AltitudeBaro;
        uav->location.altitude_geo    = l->AltitudeGeo;
        uav->location.height          = l->Height;
        uav->location.height_ref      = (uint8_t)l->HeightType;
        uav->location.speed_horizontal = l->SpeedHorizontal;
        uav->location.speed_vertical  = l->SpeedVertical;
        uav->location.direction       = l->Direction;
        uav->location.status          = (uint8_t)l->Status;
        MAP_ODID_FIELD(uav->location.h_accuracy, l->HorizAccuracy, 1);
        MAP_ODID_FIELD(uav->location.v_accuracy, l->VertAccuracy, 1);
        MAP_ODID_FIELD(uav->location.baro_accuracy, l->BaroAccuracy, 1);
        MAP_ODID_FIELD(uav->location.speed_accuracy, l->SpeedAccuracy, 1);
        MAP_ODID_FIELD(uav->location.ts_accuracy, l->TSAccuracy, 1);
        uav->location.timestamp = l->TimeStamp;
    }

    /* System Info */
    uav->system.valid = uav->uas_data.SystemValid;
    if (uav->system.valid) {
        const ODID_System_data *s = &uav->uas_data.System;
        uav->system.operator_location_type = (uint8_t)s->OperatorLocationType;
        uav->system.operator_latitude      = s->OperatorLatitude;
        uav->system.operator_longitude     = s->OperatorLongitude;
        uav->system.operator_altitude_geo  = s->OperatorAltitudeGeo;
        uav->system.area_count             = s->AreaCount;
        uav->system.area_radius            = s->AreaRadius;
        uav->system.area_ceiling           = s->AreaCeiling;
        uav->system.area_floor             = s->AreaFloor;
        uav->system.classification_type    = (uint8_t)s->ClassificationType;
        uav->system.category_eu            = (uint8_t)s->CategoryEU;
        uav->system.class_eu               = (uint8_t)s->ClassEU;
        uav->system.timestamp              = s->Timestamp;
    }

    /* Self ID */
    uav->self_id.valid = uav->uas_data.SelfIDValid;
    if (uav->self_id.valid) {
        const ODID_SelfID_data *s = &uav->uas_data.SelfID;
        uav->self_id.description_type = (uint8_t)s->DescType;
        strncpy(uav->self_id.description, s->Desc, sizeof(uav->self_id.description) - 1);
        uav->self_id.description[sizeof(uav->self_id.description) - 1] = '\0';
    }

    /* Operator ID */
    uav->operator_id.valid = uav->uas_data.OperatorIDValid;
    if (uav->operator_id.valid) {
        const ODID_OperatorID_data *o = &uav->uas_data.OperatorID;
        uav->operator_id.id_type = (uint8_t)o->OperatorIdType;
        strncpy(uav->operator_id.id, o->OperatorId, sizeof(uav->operator_id.id) - 1);
        uav->operator_id.id[sizeof(uav->operator_id.id) - 1] = '\0';
    }
    #undef MAP_ODID_FIELD
}