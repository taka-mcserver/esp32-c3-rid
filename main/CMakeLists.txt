idf_component_register(
    SRCS
        app_main.c
        crid_sniffer.c
        crid_parser_common.c
        crid_parser_gb46750.c
        crid_parser_astm.c
        crid_parser_gb42590.c
        crid_tracker.c
        crid_display.c
        crid_json.c
        crid_web.c
        crid_nvs.c
        crid_serial.c
        crid_config.c
        crid_messages.c
        crid_wifi.c
        crid_patrol.c
    INCLUDE_DIRS "."
    EMBED_TXTFILES web/index.html
    PRIV_REQUIRES opendroneid
    REQUIRES
        nvs_flash
        esp_wifi
        esp_netif
        esp_event
        log
        driver
        esp_http_server
        esp_hw_support
        esp_system
        app_update
        esp_timer
        spi_flash
        cjson
        json
        lwip
)
