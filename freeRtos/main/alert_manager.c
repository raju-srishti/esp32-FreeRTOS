#include "alert_manager.h"
#include "shared.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "ALERT";

static void alert_manager_task(void *pvParameters) {
    temp_reading_t reading;

    ESP_LOGI(TAG, "Alert manager started, priority: %d",
             uxTaskPriorityGet(NULL));

    while (1) {
        // Block until a temperature reading arrives
        if (xQueueReceive(temp_queue, &reading, portMAX_DELAY)) {

            bool is_alert = false;
            alert_event_t event = {
                .value     = reading.value,
                .timestamp = reading.timestamp,
            };

            if (reading.value >= ALERT_THRESHOLD_HIGH) {
                snprintf(event.message, sizeof(event.message),
                         "HIGH TEMP: %.1f C (threshold: %.0f C)",
                         reading.value, ALERT_THRESHOLD_HIGH);
                xEventGroupSetBits(system_events, EV_ALERT_ACTIVE);
                is_alert = true;
                ESP_LOGW(TAG, "%s", event.message);

            } else if (reading.value <= ALERT_THRESHOLD_LOW) {
                snprintf(event.message, sizeof(event.message),
                         "LOW TEMP: %.1f C (threshold: %.0f C)",
                         reading.value, ALERT_THRESHOLD_LOW);
                xEventGroupSetBits(system_events, EV_ALERT_ACTIVE);
                is_alert = true;
                ESP_LOGW(TAG, "%s", event.message);

            } else {
                // Temperature is normal — clear alert flag
                xEventGroupClearBits(system_events, EV_ALERT_ACTIVE);
            }

            if (is_alert) {
                // Store in alert history (protected by mutex)
                if (xSemaphoreTake(data_mutex, pdMS_TO_TICKS(100))) {
                    if (alert_count < MAX_ALERTS) {
                        alert_history[alert_count++] = event;
                    } else {
                        // Shift history left, add new at end
                        for (int i = 0; i < MAX_ALERTS - 1; i++) {
                            alert_history[i] = alert_history[i + 1];
                        }
                        alert_history[MAX_ALERTS - 1] = event;
                    }
                    xSemaphoreGive(data_mutex);
                }

                // Forward to webserver queue
                xQueueSend(alert_queue, &event, 0);
            }
        }
    }
}

void alert_manager_start(void) {
    // Priority 6 — HIGHER than TempTask (5)
    // This means AlertManager preempts TempTask when data arrives
    xTaskCreatePinnedToCore(alert_manager_task, "AlertMgr",
                            4096, NULL, 6, NULL, 0);
}