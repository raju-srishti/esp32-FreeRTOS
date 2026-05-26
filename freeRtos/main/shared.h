#ifndef SHARED_H
#define SHARED_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

// ─── Queue handles ────────────────────────────────────────────────────────────
extern QueueHandle_t temp_queue;    // TempTask → AlertManager
extern QueueHandle_t alert_queue;   // AlertManager → WebServer
extern QueueHandle_t stats_queue;   // SysMonitor → WebServer

// ─── Event group flags ────────────────────────────────────────────────────────
#define EV_WIFI_CONNECTED   BIT0
#define EV_SENSOR_OK        BIT1
#define EV_ALERT_ACTIVE     BIT2
#define EV_SYSTEM_READY     BIT3
extern EventGroupHandle_t system_events;

// ─── Shared data structures ───────────────────────────────────────────────────

// Temperature reading passed through temp_queue
typedef struct {
    float    value;       // temperature in Celsius
    uint32_t timestamp;   // ms since boot
} temp_reading_t;

// Alert event passed through alert_queue
typedef struct {
    float    value;           // temperature that triggered alert
    uint32_t timestamp;       // ms since boot
    char     message[64];     // human readable message
} alert_event_t;

// Per-task stats passed through stats_queue
typedef struct {
    char     name[16];        // task name
    uint32_t stack_hwm;       // stack high watermark (bytes free)
    float    cpu_percent;     // CPU usage percent (placeholder)
} task_stat_t;

// System stats snapshot passed through stats_queue
typedef struct {
    uint32_t    free_heap;        // bytes
    uint32_t    uptime_s;         // seconds since boot
    uint8_t     task_count;       // number of running tasks
    task_stat_t tasks[8];         // per task stats (max 8)
} system_stats_t;

// Temperature history for graph
#define MAX_READINGS 30
extern float         temp_history[MAX_READINGS];
extern int           temp_index;
extern int           temp_count;
extern SemaphoreHandle_t data_mutex;

// Alert history for log
#define MAX_ALERTS 10
extern alert_event_t alert_history[MAX_ALERTS];
extern int           alert_count;

// Latest system stats
extern system_stats_t latest_stats;

#endif