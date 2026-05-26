#include "nvs_flash.h"
#include "esp_log.h"
#include "shared.h"
#include "wifi.h"
#include "temperature.h"
#include "alert_manager.h"
#include "system_monitor.h"
#include "webserver.h"

static const char *TAG = "MAIN";

void app_main(void) {
    // Init NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Init shared resources
    system_events = xEventGroupCreate();
    data_mutex    = xSemaphoreCreateMutex();
    temp_queue    = xQueueCreate(10, sizeof(temp_reading_t));
    alert_queue   = xQueueCreate(10, sizeof(alert_event_t));
    stats_queue   = xQueueCreate(5,  sizeof(system_stats_t));

    ESP_LOGI(TAG, "Queues and events created");

    // Connect WiFi
    wifi_init();

    // Start all tasks
    temperature_task_start();    // priority 5, core 0
    alert_manager_start();       // priority 6, core 0
    system_monitor_start();      // priority 3, core 1
    webserver_start();           // HTTP handlers, core 1

    // Mark system ready
    xEventGroupSetBits(system_events, EV_SYSTEM_READY);

    ESP_LOGI(TAG, "All systems go! Open http://172.16.0.20");
}