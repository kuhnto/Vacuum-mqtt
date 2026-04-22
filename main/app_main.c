#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "driver/gpio.h"
#include "esp_http_server.h"
#include "esp_wifi.h"
#include "webpage.h"
#include "nvs.h"
#include "mqtt_client.h"
#include "freertos/event_groups.h"

static const char *TAG = "HA-Vac-Control";

// Event Group for Wi-Fi synchronization
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

#define GPIO_ON 0
#define GPIO_OFF 1
#define MAX_APs 10
#define NAMESPACE "storage"

// --- GPIO & MQTT HANDLERS ---

void configure_gpio_pins(void) {
    ESP_LOGI(TAG, "Configuring GPIO 0, 1, and 3 as Open-Drain outputs");

    gpio_config_t io_conf = {
        // Create a bit mask for pins 0, 1, and 3
        .pin_bit_mask = (1ULL << GPIO_NUM_0) | (1ULL << GPIO_NUM_1) | (1ULL << GPIO_NUM_3),
        .mode = GPIO_MODE_OUTPUT_OD,          // Open-Drain mode
        .pull_up_en = GPIO_PULLUP_DISABLE,    // Rely on external 3.3V pull-up
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    
    gpio_config(&io_conf);

    // Default them to '1' (Floating/High Impedance) so they don't pull to ground yet
    gpio_set_level(GPIO_NUM_0, 1);
    gpio_set_level(GPIO_NUM_1, 1);
    gpio_set_level(GPIO_NUM_3, 1);
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;

    switch (event_id) {
        case MQTT_EVENT_CONNECTED:
            esp_mqtt_client_subscribe(client, "vacuum/commands", 0);
            break;

        case MQTT_EVENT_DATA: { // <--- Added braces for scope
            gpio_num_t target_pin = GPIO_NUM_NC; // Default to 'Not Connected'

            // Clean, length-validated structural checks
            if (event->data_len == 4 && strncmp(event->data, "DOCK", 4) == 0) {
                target_pin = GPIO_NUM_0;
            } else if (event->data_len == 5 && strncmp(event->data, "CLEAN", 5) == 0) {
                target_pin = GPIO_NUM_1;
            } else if (event->data_len == 3 && strncmp(event->data, "MAX", 3) == 0) {
                target_pin = GPIO_NUM_3;
            }

            // Centralized Execution Logic
            if (target_pin != GPIO_NUM_NC) {
                ESP_LOGI(TAG, "Triggering Pin %d", target_pin);
                
                // Structurally, offloading the pulse to a task is better
                // But if you keep it here, at least it's only in one place!
                gpio_set_level(target_pin, 0); 
                vTaskDelay(pdMS_TO_TICKS(100));
                gpio_set_level(target_pin, 1);
            }
            break;
        }

        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT Error Occurred");
            break;

        default:
            ESP_LOGD(TAG, "Other event id:%d", event_id);
            break;
    }
}
// --- NVS LOGIC ---

void save_settings(const char* ip, const char* user, const char* pass, const char* wifi_AP, const char* wifi_password) {
    nvs_handle_t my_handle;
    if (nvs_open(NAMESPACE, NVS_READWRITE, &my_handle) == ESP_OK) {
        nvs_set_str(my_handle, "mqtt_ip", ip);
        nvs_set_str(my_handle, "mqtt_user", user);
        nvs_set_str(my_handle, "mqtt_pass", pass);
        nvs_set_str(my_handle, "wifi_AP", wifi_AP);
        if (strlen(wifi_password) > 0) nvs_set_str(my_handle, "wifi_password", wifi_password);
        nvs_commit(my_handle);
        nvs_close(my_handle);
    }
}

void load_settings(char* ip, char* user, char* pass, char* wifi_AP, char* wifi_password) {
    nvs_handle_t my_handle;
    size_t size;
    ip[0] = user[0] = pass[0] = wifi_AP[0] = wifi_password[0] = '\0';
    if (nvs_open(NAMESPACE, NVS_READONLY, &my_handle) == ESP_OK) {
        nvs_get_str(my_handle, "mqtt_ip", NULL, &size); nvs_get_str(my_handle, "mqtt_ip", ip, &size);
        nvs_get_str(my_handle, "mqtt_user", NULL, &size); nvs_get_str(my_handle, "mqtt_user", user, &size);
        nvs_get_str(my_handle, "mqtt_pass", NULL, &size); nvs_get_str(my_handle, "mqtt_pass", pass, &size);
        nvs_get_str(my_handle, "wifi_AP", NULL, &size); nvs_get_str(my_handle, "wifi_AP", wifi_AP, &size);
        nvs_get_str(my_handle, "wifi_password", NULL, &size); nvs_get_str(my_handle, "wifi_password", wifi_password, &size);
        nvs_close(my_handle);
    }
}

// --- WIFI & WEB LOGIC ---

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) esp_wifi_connect();
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) esp_wifi_connect();
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
}

void get_wifi_list_html(char *list_buf, size_t max_len) {
    uint16_t number = MAX_APs;
    wifi_ap_record_t ap_info[MAX_APs];
    uint16_t ap_count = 0;
    esp_wifi_scan_start(NULL, true);
    esp_wifi_scan_get_ap_num(&ap_count);
    esp_wifi_scan_get_ap_records(&number, ap_info);
    
    strcat(list_buf, "<label>Select Wi-Fi Network</label><select name='ssid'>");
    for (int i = 0; i < number && i < ap_count; i++) {
        char option[128];
        snprintf(option, sizeof(option), "<option value='%s'>%s</option>", (char*)ap_info[i].ssid, (char*)ap_info[i].ssid);
        strcat(list_buf, option);
    }
    strcat(list_buf, "</select>");
}

void url_decode(char *src) {
    char *dst = src;
    while (*src) {
        if (*src == '%' && src[1] && src[2]) {
            int val; sscanf(src + 1, "%2x", &val);
            *dst++ = (char)val; src += 3;
        } else if (*src == '+') { *dst++ = ' '; src++; }
        else { *dst++ = *src++; }
    }
    *dst = '\0';
}

esp_err_t settings_get_handler(httpd_req_t *req) {
    char ip[32], user[32], pass[32], w_ap[32], w_pw[32];
    load_settings(ip, user, pass, w_ap, w_pw);
    char wifi_list[1024] = {0};
    get_wifi_list_html(wifi_list, sizeof(wifi_list));
    char *out_buf = malloc(3072);
    snprintf(out_buf, 3072, html_template, wifi_list, ip, user, pass);
    httpd_resp_send(req, out_buf, HTTPD_RESP_USE_STRLEN);
    free(out_buf);
    return ESP_OK;
}

esp_err_t settings_post_handler(httpd_req_t *req) {
    char buf[256] = {0};
    httpd_req_recv(req, buf, sizeof(buf) - 1);
    char ip[64]={0}, user[32]={0}, pass[32]={0}, ssid[32]={0}, wifi_pass[64]={0};
    httpd_query_key_value(buf, "ip", ip, 64);
    httpd_query_key_value(buf, "user", user, 32);
    httpd_query_key_value(buf, "pass", pass, 32);
    httpd_query_key_value(buf, "ssid", ssid, 32);
    httpd_query_key_value(buf, "wifi_pass", wifi_pass, 64);
    url_decode(ip); url_decode(user); url_decode(pass); url_decode(ssid); url_decode(wifi_pass);
    save_settings(ip, user, pass, ssid, wifi_pass);
    httpd_resp_sendstr(req, "Saved! Rebooting...");
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
    return ESP_OK;
}

// --- APP STARTUP ---

void mqtt_app_start(void) {
    char ip[32], user[32], pass[32], w_ap[32], w_pw[32];
    load_settings(ip, user, pass, w_ap, w_pw);
    char uri[64]; snprintf(uri, 64, "mqtt://%s", ip);
    const esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = uri,
        .credentials = { .username = user, .authentication.password = pass }
    };
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}

// Move this above app_main or put in a header
void start_settings_server(void) {
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    
    // We increase the stack size slightly because we are doing 
    // a Wi-Fi scan within the handler, which is memory-intensive.
    config.stack_size = 8192; 

    static httpd_uri_t uri_get = { .uri = "/", .method = HTTP_GET, .handler = settings_get_handler };
    static httpd_uri_t uri_post = { .uri = "/save", .method = HTTP_POST, .handler = settings_post_handler };

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &uri_get);
        httpd_register_uri_handler(server, &uri_post);
        ESP_LOGI(TAG, "Web Server started on STA/AP interface.");
    }
}

void app_main(void) {
    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();
    s_wifi_event_group = xEventGroupCreate();
    
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL);

    char ip[32], user[32], pass[32], wifi_AP[32], wifi_password[32];
    load_settings(ip, user, pass, wifi_AP, wifi_password);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    if (strlen(wifi_AP) > 0) {
        // --- STATION MODE ---
        esp_netif_create_default_wifi_sta();
        wifi_config_t sta_cfg = { .sta = { .threshold.authmode = WIFI_AUTH_WPA2_PSK } };
        strncpy((char*)sta_cfg.sta.ssid, wifi_AP, 32);
        strncpy((char*)sta_cfg.sta.password, wifi_password, 64);
        
        esp_wifi_set_mode(WIFI_MODE_STA);
        esp_wifi_set_config(WIFI_IF_STA, &sta_cfg);
        esp_wifi_start();

        // 1. Wait for connection
        ESP_LOGI(TAG, "Connecting to Home Wi-Fi...");
        xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdFALSE, pdMS_TO_TICKS(15000));
        
        // 2. Start Services
        configure_gpio_pins();
        start_settings_server(); // <-- Now starts in STA mode
        mqtt_app_start();

    } else {
        // --- SETUP AP MODE ---
        esp_netif_create_default_wifi_ap();
        esp_netif_create_default_wifi_sta(); // Keep for scanning
        
        wifi_config_t ap_cfg = { .ap = { .ssid = "ESP32_Vac_Setup", .channel = 1, .password = "password123", .authmode = WIFI_AUTH_WPA2_PSK, .max_connection = 4 } };
        
        esp_wifi_set_mode(WIFI_MODE_APSTA);
        esp_wifi_set_config(WIFI_IF_AP, &ap_cfg);
        esp_wifi_start();

        start_settings_server(); // <-- Starts in AP mode
    }
}