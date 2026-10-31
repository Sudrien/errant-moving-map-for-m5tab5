/*
 * wifijoin.c -- see wifijoin.h. The body is portal.c's try_join() as it
 * was before 5122, less the portal's own state.
 *
 * defeatist-music-player-for-m5tab5's main/wifijoin.c at 74535ca, but
 * the PSK, which is the network handler's since 0043.
 *
 * SPDX-License-Identifier: MIT
 */
#include "wifijoin.h"

#include <string.h>

#include "esp_log.h"
#include "esp_wifi.h"

#include "wifi.h"
#include "wifistore.h"

/* 0043: the WPA2 PSK is feckless_network_handler's wifi_psk()
 * (feckless-libs' 6003), the derivation this file did, so the ELF does
 * not reach Mbed TLS itself. 0 on success, as before. */
static int wpa_psk(const char *pass, const char *ssid, uint8_t key[32])
{
    return wifi_psk(ssid, pass, key) == ESP_OK ? 0 : -1;
}

portalweb_net_t wifijoin_net(uint8_t auth)
{
    switch ((wifi_auth_mode_t)auth) {
    case WIFI_AUTH_WPA3_PSK:
    case WIFI_AUTH_WPA2_WPA3_PSK:
        return PORTALWEB_NET_WPA3_CAPABLE;
    case WIFI_AUTH_WPA_PSK:
    case WIFI_AUTH_WPA2_PSK:
    case WIFI_AUTH_WPA_WPA2_PSK:
        return PORTALWEB_NET_WPA2_ONLY;
    default:
        return PORTALWEB_NET_UNKNOWN;
    }
}

esp_err_t wifijoin_try(const char *TAG, const char *ssid, const char *pass,
                       portalweb_net_t net)
{
    const portalweb_check_t kind = portalweb_check(ssid, pass);
    const portalweb_plan_t plan = portalweb_join_plan(kind, net);
    if (plan == PORTALWEB_TRY_NONE) return ESP_ERR_INVALID_ARG;

    ESP_LOGI(TAG, "trying %.32s (%s)", ssid,
             plan == PORTALWEB_TRY_AS_TYPED        ? "PSK as typed" :
             plan == PORTALWEB_TRY_PASSPHRASE_ONLY ? "passphrase only: WPA3-capable network" :
             net  == PORTALWEB_NET_WPA2_ONLY       ? "PSK, then passphrase: WPA2 network" :
                                                     "PSK, then passphrase: not in the scan");

    esp_err_t err = ESP_ERR_INVALID_ARG;
    const char *stored = NULL;
    bool is_psk = false;
    char hex[WIFISTORE_SECRET_MAX + 1] = { 0 };

    if (plan == PORTALWEB_TRY_AS_TYPED) {
        err = wifi_join(ssid, pass, WIFI_JOIN_TIMEOUT_MS);
        stored = pass;
        is_psk = true;
    } else if (plan == PORTALWEB_TRY_PSK_THEN_PASSPHRASE) {
        uint8_t key[32];
        const int rc = wpa_psk(pass, ssid, key);
        if (rc == 0) {
            portalweb_hex(key, sizeof(key), hex);
            err = wifi_join(ssid, hex, WIFI_JOIN_TIMEOUT_MS);
            stored = hex;
            is_psk = true;
        } else {
            ESP_LOGW(TAG, "PBKDF2 failed (%d); trying the passphrase", rc);
            err = ESP_ERR_WIFI_PASSWORD;
        }
        memset(key, 0, sizeof(key));

        /* Refused, not absent: most likely an AP that insists on SAE,
         * which cannot use a precomputed key. */
        if (err == ESP_ERR_WIFI_PASSWORD) {
            err = wifi_join(ssid, pass, WIFI_JOIN_TIMEOUT_MS);
            stored = pass;
            is_psk = false;
        }
    } else if (plan == PORTALWEB_TRY_PASSPHRASE_ONLY) {
        err = wifi_join(ssid, pass, WIFI_JOIN_TIMEOUT_MS);
        stored = pass;
        is_psk = false;
    }

    if (err == ESP_OK) {
        const esp_err_t saved = wifistore_save(ssid, stored, is_psk);
        if (saved != ESP_OK) {
            ESP_LOGE(TAG, "joined but could not save: %s", esp_err_to_name(saved));
            err = saved;
        } else {
            ESP_LOGI(TAG, "saved %.32s as %s", ssid, is_psk ? "PSK" : "passphrase");
        }
    }
    memset(hex, 0, sizeof(hex));
    return err;
}
