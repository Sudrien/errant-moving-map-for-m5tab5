/* SPDX-License-Identifier: MIT
 *
 * u-errant-0002: what a box does not answer, as the app's own functions.
 *
 * The boxed ELF is the map's sources, so it still calls the network, the
 * GNSS's UART, the IMU's I2C device, a GPIO interrupt and the flash's
 * partitions, which a feckless box (feckless-libs' umode branch, to
 * u-feckless-0061) has no host call for. Each is defined here to fail the
 * way the map already copes with on a trusted run:
 *
 *   the network      not started, down and never coming up: no radio, no
 *                    saved networks, no sockets, no HTTP client or server.
 *                    The map is the offline map that way already when no
 *                    network is saved: tiles from the card or a USB drive,
 *                    no remote tiles, no setup portal, no Wi-Fi location,
 *                    no NTP (so no clock check but the RTC's).
 *   the UART         no driver: gnss_start() says so and the map has no
 *                    position but the last one it saved.
 *   the I2C device   nothing answers at the BMI270's address: no
 *                    accelerometer, and the map dims on touch and distance
 *                    alone, as it does without the configuration image.
 *   GPIO             no pin configured and no interrupt: no PPS count.
 *   partitions       none: no M5Launcher in the factory slot, so nothing
 *                    to import from it.
 *
 * Each of these comes out of this file when a box answers it -- the box's
 * call then replaces it at the next build, and boxcheck says which are
 * left. Not a stub of the map's own code: every name here is a library's.
 */
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "esp_err.h"

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/uart.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_http_server.h"
#include "esp_netif.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"

#include "ethernet.h"
#include "feckless_net.h"
#include "wifi.h"
#include "wifistore.h"

static bool empty(char *out, size_t n)
{
    if (out && n) out[0] = '\0';
    return false;
}

/* ---- the GNSS's UART --------------------------------------------------- */

esp_err_t uart_driver_install(uart_port_t uart_num, int rx_buffer_size, int tx_buffer_size,
                              int queue_size, QueueHandle_t *uart_queue, int intr_alloc_flags)
{
    (void)uart_num; (void)rx_buffer_size; (void)tx_buffer_size;
    (void)queue_size; (void)uart_queue; (void)intr_alloc_flags;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t uart_param_config(uart_port_t uart_num, const uart_config_t *uart_config)
{
    (void)uart_num; (void)uart_config;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t _uart_set_pin6(uart_port_t uart_num, int tx_io_num, int rx_io_num, int rts_io_num,
                         int cts_io_num, int dtr_io_num, int dsr_io_num)
{
    (void)uart_num; (void)tx_io_num; (void)rx_io_num; (void)rts_io_num;
    (void)cts_io_num; (void)dtr_io_num; (void)dsr_io_num;
    return ESP_ERR_NOT_SUPPORTED;
}

/* Never reached -- gnss_start() stops at the install -- but a reader that
 * was would spin on an instant -1, so it waits as the driver would. */
int uart_read_bytes(uart_port_t uart_num, void *buf, uint32_t length, TickType_t ticks_to_wait)
{
    (void)uart_num; (void)buf; (void)length;
    vTaskDelay(ticks_to_wait ? ticks_to_wait : 1);
    return -1;
}

int uart_write_bytes(uart_port_t uart_num, const void *src, size_t size)
{
    (void)uart_num; (void)src; (void)size;
    return -1;
}

esp_err_t uart_wait_tx_done(uart_port_t uart_num, TickType_t ticks_to_wait)
{
    (void)uart_num; (void)ticks_to_wait;
    return ESP_ERR_NOT_SUPPORTED;
}

/* ---- the IMU's I2C device ---------------------------------------------- */

esp_err_t i2c_master_probe(i2c_master_bus_handle_t bus_handle, uint16_t address, int xfer_timeout_ms)
{
    (void)bus_handle; (void)address; (void)xfer_timeout_ms;
    return ESP_ERR_NOT_FOUND;
}

esp_err_t i2c_master_bus_add_device(i2c_master_bus_handle_t bus_handle,
                                    const i2c_device_config_t *dev_config,
                                    i2c_master_dev_handle_t *ret_handle)
{
    (void)bus_handle; (void)dev_config;
    if (ret_handle) *ret_handle = NULL;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t i2c_master_bus_rm_device(i2c_master_dev_handle_t handle)
{
    (void)handle;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t i2c_master_transmit(i2c_master_dev_handle_t i2c_dev, const uint8_t *write_buffer,
                              size_t write_size, int xfer_timeout_ms)
{
    (void)i2c_dev; (void)write_buffer; (void)write_size; (void)xfer_timeout_ms;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t i2c_master_transmit_receive(i2c_master_dev_handle_t i2c_dev, const uint8_t *write_buffer,
                                      size_t write_size, uint8_t *read_buffer, size_t read_size,
                                      int xfer_timeout_ms)
{
    (void)i2c_dev; (void)write_buffer; (void)write_size;
    (void)read_buffer; (void)read_size; (void)xfer_timeout_ms;
    return ESP_ERR_NOT_SUPPORTED;
}

/* ---- GPIO: the PPS pin and its interrupt ------------------------------- */

esp_err_t gpio_config(const gpio_config_t *pGPIOConfig)
{
    (void)pGPIOConfig;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t gpio_install_isr_service(int intr_alloc_flags)
{
    (void)intr_alloc_flags;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t gpio_isr_handler_add(gpio_num_t gpio_num, gpio_isr_t isr_handler, void *args)
{
    (void)gpio_num; (void)isr_handler; (void)args;
    return ESP_ERR_NOT_SUPPORTED;
}

/* ---- the flash's partitions -------------------------------------------- */

const esp_partition_t *esp_ota_get_running_partition(void) { return NULL; }

esp_partition_iterator_t esp_partition_find(esp_partition_type_t type,
                                            esp_partition_subtype_t subtype, const char *label)
{
    (void)type; (void)subtype; (void)label;
    return NULL;
}

const esp_partition_t *esp_partition_find_first(esp_partition_type_t type,
                                                esp_partition_subtype_t subtype, const char *label)
{
    (void)type; (void)subtype; (void)label;
    return NULL;
}

const esp_partition_t *esp_partition_get(esp_partition_iterator_t iterator)
{
    (void)iterator;
    return NULL;
}

esp_partition_iterator_t esp_partition_next(esp_partition_iterator_t iterator)
{
    (void)iterator;
    return NULL;
}

void esp_partition_iterator_release(esp_partition_iterator_t iterator) { (void)iterator; }

esp_err_t esp_partition_read(const esp_partition_t *partition, size_t src_offset, void *dst,
                             size_t size)
{
    (void)partition; (void)src_offset; (void)dst; (void)size;
    return ESP_ERR_NOT_SUPPORTED;
}

/* ---- the network handler ----------------------------------------------- */

bool feckless_net_started(void) { return false; }
void feckless_net_set_hooks(const feckless_net_hooks_t *hooks) { (void)hooks; }

bool net_online(void) { return false; }
void net_route_describe(char *out, size_t out_size) { empty(out, out_size); }

bool wifi_up(void) { return false; }
void wifi_request_apply(void) {}
bool wifi_ntp_synced(void) { return false; }
bool wifi_sta_ssid(char *out, size_t out_size) { return empty(out, out_size); }
const char *wifi_auth_name(uint8_t auth) { (void)auth; return "?"; }

esp_err_t wifi_join(const char *ssid, const char *secret, uint32_t timeout_ms)
{
    (void)ssid; (void)secret; (void)timeout_ms;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t wifi_psk(const char *ssid, const char *passphrase, uint8_t key[32])
{
    (void)ssid; (void)passphrase; (void)key;
    return ESP_ERR_NOT_SUPPORTED;
}

int wifi_scan_list(wifi_seen_t *out, int max) { (void)out; (void)max; return 0; }
int wifi_scan_list_quiet(wifi_seen_t *out, int max) { (void)out; (void)max; return 0; }

esp_err_t wifi_ap_begin(const char *ssid) { (void)ssid; return ESP_ERR_NOT_SUPPORTED; }
esp_err_t wifi_ap_end(void) { return ESP_OK; }
int wifi_ap_clients(void) { return 0; }
struct esp_netif_obj *wifi_ap_netif(void) { return NULL; }

int wifistore_count(void) { return 0; }

esp_err_t wifistore_save(const char *ssid, const char *secret, bool is_psk)
{
    (void)ssid; (void)secret; (void)is_psk;
    return ESP_ERR_NOT_SUPPORTED;
}

/* ---- esp_netif, esp_wifi ----------------------------------------------- */

esp_err_t esp_wifi_get_mac(wifi_interface_t ifx, uint8_t mac[6])
{
    (void)ifx;
    if (mac) memset(mac, 0, 6);
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t esp_netif_get_ip_info(esp_netif_t *esp_netif, esp_netif_ip_info_t *ip_info)
{
    (void)esp_netif;
    if (ip_info) memset(ip_info, 0, sizeof(*ip_info));
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t esp_netif_set_dns_info(esp_netif_t *esp_netif, esp_netif_dns_type_t type,
                                 esp_netif_dns_info_t *dns)
{
    (void)esp_netif; (void)type; (void)dns;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t esp_netif_dhcps_start(esp_netif_t *esp_netif) { (void)esp_netif; return ESP_ERR_NOT_SUPPORTED; }
esp_err_t esp_netif_dhcps_stop(esp_netif_t *esp_netif) { (void)esp_netif; return ESP_ERR_NOT_SUPPORTED; }

esp_err_t esp_netif_dhcps_option(esp_netif_t *esp_netif, esp_netif_dhcp_option_mode_t opt_op,
                                 esp_netif_dhcp_option_id_t opt_id, void *opt_val, uint32_t opt_len)
{
    (void)esp_netif; (void)opt_op; (void)opt_id; (void)opt_val; (void)opt_len;
    return ESP_ERR_NOT_SUPPORTED;
}

/* ---- lwIP sockets ------------------------------------------------------ */

static int no_socket(void)
{
    errno = ENETDOWN;
    return -1;
}

int lwip_socket(int domain, int type, int protocol)
{
    (void)domain; (void)type; (void)protocol;
    return no_socket();
}

int lwip_bind(int s, const struct sockaddr *name, socklen_t namelen)
{
    (void)s; (void)name; (void)namelen;
    return no_socket();
}

int lwip_setsockopt(int s, int level, int optname, const void *optval, socklen_t optlen)
{
    (void)s; (void)level; (void)optname; (void)optval; (void)optlen;
    return no_socket();
}

ssize_t lwip_recvfrom(int s, void *mem, size_t len, int flags, struct sockaddr *from,
                      socklen_t *fromlen)
{
    (void)s; (void)mem; (void)len; (void)flags; (void)from; (void)fromlen;
    return no_socket();
}

ssize_t lwip_sendto(int s, const void *dataptr, size_t size, int flags, const struct sockaddr *to,
                    socklen_t tolen)
{
    (void)s; (void)dataptr; (void)size; (void)flags; (void)to; (void)tolen;
    return no_socket();
}

/* Only ever a socket's here: the map's files are FatFs's and stdio's. */
int close(int fd)
{
    (void)fd;
    errno = EBADF;
    return -1;
}

u32_t ipaddr_addr(const char *cp) { (void)cp; return IPADDR_NONE; }

/* ---- esp_http_client, and the certificate bundle ----------------------- */

esp_err_t esp_crt_bundle_attach(void *conf) { (void)conf; return ESP_ERR_NOT_SUPPORTED; }

esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t *config)
{
    (void)config;
    return NULL;
}

esp_err_t esp_http_client_set_url(esp_http_client_handle_t client, const char *url)
{
    (void)client; (void)url;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t esp_http_client_set_header(esp_http_client_handle_t client, const char *key,
                                     const char *value)
{
    (void)client; (void)key; (void)value;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t esp_http_client_open(esp_http_client_handle_t client, int write_len)
{
    (void)client; (void)write_len;
    return ESP_ERR_NOT_SUPPORTED;
}

int64_t esp_http_client_fetch_headers(esp_http_client_handle_t client) { (void)client; return -1; }
int esp_http_client_get_status_code(esp_http_client_handle_t client) { (void)client; return 0; }

int esp_http_client_read(esp_http_client_handle_t client, char *buffer, int len)
{
    (void)client; (void)buffer; (void)len;
    return -1;
}

bool esp_http_client_is_complete_data_received(esp_http_client_handle_t client)
{
    (void)client;
    return false;
}

esp_err_t esp_http_client_cleanup(esp_http_client_handle_t client) { (void)client; return ESP_OK; }

/* ---- esp_http_server: the setup portal --------------------------------- */

esp_err_t httpd_start(httpd_handle_t *handle, const httpd_config_t *config)
{
    (void)config;
    if (handle) *handle = NULL;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t httpd_stop(httpd_handle_t handle) { (void)handle; return ESP_OK; }

esp_err_t httpd_register_uri_handler(httpd_handle_t handle, const httpd_uri_t *uri_handler)
{
    (void)handle; (void)uri_handler;
    return ESP_ERR_NOT_SUPPORTED;
}

bool httpd_uri_match_wildcard(const char *uri_template, const char *uri_to_match, size_t match_upto)
{
    (void)uri_template; (void)uri_to_match; (void)match_upto;
    return false;
}

int httpd_req_recv(httpd_req_t *r, char *buf, size_t buf_len)
{
    (void)r; (void)buf; (void)buf_len;
    return -1;
}

esp_err_t httpd_resp_send(httpd_req_t *r, const char *buf, ssize_t buf_len)
{
    (void)r; (void)buf; (void)buf_len;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t httpd_resp_send_chunk(httpd_req_t *r, const char *buf, ssize_t buf_len)
{
    (void)r; (void)buf; (void)buf_len;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t httpd_resp_set_hdr(httpd_req_t *r, const char *field, const char *value)
{
    (void)r; (void)field; (void)value;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t httpd_resp_set_status(httpd_req_t *r, const char *status)
{
    (void)r; (void)status;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t httpd_resp_set_type(httpd_req_t *r, const char *type)
{
    (void)r; (void)type;
    return ESP_ERR_NOT_SUPPORTED;
}
