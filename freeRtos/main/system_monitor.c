#include "system_monitor.h"
#include "shared.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include <string.h>

static const char *TAG = "SYSMON";

static TaskHandle_t temp_task_handle   = NULL;
static TaskHandle_t alert_task_handle  = NULL;
static TaskHandle_t sysmon_task_handle = NULL;

static void system_monitor_task(void *pvParameters) {
    ESP_LOGI(TAG, "System monitor started, priority: %d",
             uxTaskPriorityGet(NULL));

    while (1) {
        system_stats_t stats = {0};

        // Heap and uptime
        stats.free_heap  = heap_caps_get_free_size(MALLOC_CAP_8BIT);
        stats.uptime_s   = (uint32_t)(esp_timer_get_time() / 1000000);
        stats.task_count = (uint8_t)uxTaskGetNumberOfTasks();

        // Known tasks with their handles
        struct {
            const char   *name;
            TaskHandle_t  handle;
        } known_tasks[] = {
            { "TempTask", temp_task_handle  },
            { "AlertMgr", alert_task_handle },
            { "SysMon",   sysmon_task_handle},
        };

        int i = 0;
        for (int t = 0; t < 3; t++) {
            if (known_tasks[t].handle != NULL) {
                strncpy(stats.tasks[i].name, known_tasks[t].name, 15);
                stats.tasks[i].stack_hwm =
                    uxTaskGetStackHighWaterMark(known_tasks[t].handle) * 4;
                stats.tasks[i].cpu_percent = 0.0f;
                i++;
            }
        }
        stats.task_count = i;

        // Log to serial
        ESP_LOGI(TAG, "Free heap: %lu B | Uptime: %lu s | Tasks running: %d",
                 stats.free_heap, stats.uptime_s,
                 (uint8_t)uxTaskGetNumberOfTasks());

        for (int j = 0; j < i; j++) {
            ESP_LOGI(TAG, "  %-12s  stack free: %lu B",
                     stats.tasks[j].name,
                     stats.tasks[j].stack_hwm);
        }

        // Update global stats safely
        if (xSemaphoreTake(data_mutex, pdMS_TO_TICKS(100))) {
            latest_stats = stats;
            xSemaphoreGive(data_mutex);
        }

        xQueueSend(stats_queue, &stats, 0);

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void system_monitor_set_handles(TaskHandle_t temp,
                                TaskHandle_t alert,
                                TaskHandle_t sysmon) {
    temp_task_handle   = temp;
    alert_task_handle  = alert;
    sysmon_task_handle = sysmon;
}

void system_monitor_start(void) {
    xTaskCreatePinnedToCore(system_monitor_task, "SysMon",
                            4096, NULL, 3, &sysmon_task_handle, 1);
}