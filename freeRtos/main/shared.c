#include "shared.h"

// Queues
QueueHandle_t temp_queue;
QueueHandle_t alert_queue;
QueueHandle_t stats_queue;

// Event group
EventGroupHandle_t system_events;

// Temperature history
float temp_history[MAX_READINGS] = {0};
int   temp_index                 = 0;
int   temp_count                 = 0;
SemaphoreHandle_t data_mutex;

// Alert history
alert_event_t alert_history[MAX_ALERTS] = {0};
int           alert_count               = 0;

// Latest system stats
system_stats_t latest_stats = {0};