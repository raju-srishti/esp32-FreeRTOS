#include "temperature.h"
#include "shared.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <stdlib.h>

static const char *TAG = "TEMPERATURE";

static void temperature_task(void *pvParameters) {
    float temp = 25.0f;

    ESP_LOGI(TAG, "Temperature task started, priority: %d",
             uxTaskPriorityGet(NULL));

    // Signal sensor is ready
    xEventGroupSetBits(system_events, EV_SENSOR_OK);

    while (1) {
        // Simulate temperature fluctuation
        float delta = ((float)(rand() % 11) - 5) / 10.0f;
        temp += delta;
        if (temp < 20.0f) temp = 20.0f;
        if (temp > 45.0f) temp = 45.0f;

        // Build reading struct
        temp_reading_t reading = {
            .value     = temp,
            .timestamp = (uint32_t)(esp_timer_get_time() / 1000),
        };

        ESP_LOGI(TAG, "Reading: %.1f C @ %lu ms", temp, reading.timestamp);

        // Store in history (protected by mutex)
        if (xSemaphoreTake(data_mutex, pdMS_TO_TICKS(100))) {
            temp_history[temp_index] = temp;
            temp_index = (temp_index + 1) % MAX_READINGS;
            if (temp_count < MAX_READINGS) temp_count++;
            xSemaphoreGive(data_mutex);
        }

        // Send to alert manager via queue (don't wait if full)
        if (xQueueSend(temp_queue, &reading, 0) != pdTRUE) {
            ESP_LOGW(TAG, "temp_queue full!");
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void temperature_task_start(void) {
    xTaskCreatePinnedToCore(temperature_task, "TempTask",
                            4096, NULL, 5, NULL, 0);
}