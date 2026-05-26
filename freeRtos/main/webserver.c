#include "webserver.h"
#include "shared.h"
#include "alert_manager.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "WEBSERVER";

// ─── Dashboard HTML ───────────────────────────────────────────────────────────
static esp_err_t dashboard_handler(httpd_req_t *req) {
    const char *html =
"<!DOCTYPE html><html><head><meta charset='UTF-8'><title>ESP32 RTOS System Monitor</title>"
"<meta name='viewport' content='width=device-width,initial-scale=1'>"
"<style>"
"*{box-sizing:border-box;margin:0;padding:0}"
"body{font-family:Arial,sans-serif;background:#0d1117;color:#c9d1d9;padding:16px}"
"h1{color:#58a6ff;text-align:center;margin-bottom:20px;font-size:1.4em}"
".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(280px,1fr));gap:16px}"
".card{background:#161b22;border:1px solid #30363d;border-radius:10px;padding:16px}"
".card h2{color:#58a6ff;font-size:0.9em;margin-bottom:12px;text-transform:uppercase}"
".big{font-size:2.5em;font-weight:bold;text-align:center;padding:10px}"
".ok{color:#3fb950}.warn{color:#f85149}"
"canvas{width:100%;height:120px}"
".alert-item{background:#21262d;border-left:3px solid #f85149;"
"padding:8px;margin:4px 0;border-radius:4px;font-size:0.8em}"
"table{width:100%;border-collapse:collapse;font-size:0.8em}"
"th{color:#8b949e;text-align:left;padding:4px 8px;border-bottom:1px solid #30363d}"
"td{padding:4px 8px;border-bottom:1px solid #21262d}"
".stat{display:flex;justify-content:space-between;padding:6px 0;"
"border-bottom:1px solid #21262d;font-size:0.85em}"
".stat span:last-child{color:#3fb950}"
"</style></head><body>"
"<h1>ESP32 FreeRTOS System Monitor</h1>"
"<div class='grid'>"

// Temperature card
"<div class='card'><h2>Temperature</h2>"
"<div class='big' id='temp'>--.-°C</div>"
"<canvas id='chart'></canvas></div>"

// System stats card
"<div class='card'><h2>System</h2>"
"<div class='stat'><span>Free Heap</span><span id='heap'>--</span></div>"
"<div class='stat'><span>Uptime</span><span id='uptime'>--</span></div>"
"<div class='stat'><span>Tasks Running</span><span id='tasks'>--</span></div>"
"<div class='stat'><span>WiFi</span><span class='ok'>Connected</span></div>"
"<div class='stat'><span>Sensor</span><span class='ok'>OK</span></div>"
"</div>"

// Task table card
"<div class='card'><h2>FreeRTOS Tasks</h2>"
"<table><thead><tr><th>Task</th><th>Stack Free</th><th>CPU%</th></tr></thead>"
"<tbody id='task-table'></tbody></table></div>"

// Alert log card
"<div class='card'><h2>Alert Log</h2>"
"<div id='alerts'><p style='color:#8b949e;font-size:0.85em'>No alerts yet</p></div>"
"</div>"

"</div>"
"<script>"
"var temps=[];"
"function fmtTime(s){var h=Math.floor(s/3600),m=Math.floor((s%3600)/60),sc=s%60;"
"return h+'h '+m+'m '+sc+'s';}"
"function drawChart(){"
"var c=document.getElementById('chart');"
"c.width=c.offsetWidth;c.height=120;"
"var ctx=c.getContext('2d'),w=c.width,h=c.height,p=20;"
"ctx.fillStyle='#0d1117';ctx.fillRect(0,0,w,h);"
"if(temps.length<2)return;"
"var mn=Math.min(...temps)-1,mx=Math.max(...temps)+1;"
"ctx.strokeStyle='#30363d';ctx.lineWidth=1;"
"ctx.beginPath();ctx.moveTo(p,p);ctx.lineTo(p,h-p);ctx.lineTo(w-p,h-p);ctx.stroke();"
"ctx.strokeStyle='#58a6ff';ctx.lineWidth=2;ctx.beginPath();"
"temps.forEach(function(v,i){"
"var x=p+(i/(temps.length-1))*(w-2*p);"
"var y=h-p-(v-mn)/(mx-mn)*(h-2*p);"
"i===0?ctx.moveTo(x,y):ctx.lineTo(x,y);});"
"ctx.stroke();"
"ctx.fillStyle='#8b949e';ctx.font='10px Arial';"
"ctx.fillText(mx.toFixed(1)+'C',2,p+4);"
"ctx.fillText(mn.toFixed(1)+'C',2,h-p+4);}"
"function update(){"
"fetch('/data').then(r=>r.json()).then(function(d){"
"temps=d.history;"
"var t=d.current;"
"var el=document.getElementById('temp');"
"el.textContent=t.toFixed(1)+'°C';"
"el.className='big '+(t>=35||t<=21?'warn':'ok');"
"document.getElementById('heap').textContent=(d.heap/1024).toFixed(1)+' KB';"
"document.getElementById('uptime').textContent=fmtTime(d.uptime);"
"document.getElementById('tasks').textContent=d.task_count;"
"var tb=document.getElementById('task-table');"
"tb.innerHTML=d.tasks.map(function(t){"
"return '<tr><td>'+t.name+'</td><td>'+t.stack+'B</td><td>'+t.cpu+'%</td></tr>';}).join('');"
"var al=document.getElementById('alerts');"
"if(d.alerts&&d.alerts.length>0){"
"al.innerHTML=d.alerts.map(function(a){"
"return '<div class=\"alert-item\">'+a.msg+'<br><small>@ '+a.ts+'ms</small></div>';"
"}).join('');} "
"drawChart();});"
"setTimeout(update,2000);}"
"update();"
"</script></body></html>";

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html, strlen(html));
    return ESP_OK;
}

// ─── JSON Data Endpoint ───────────────────────────────────────────────────────
static esp_err_t data_handler(httpd_req_t *req) {
    static char json[2048];
    static char history_buf[512];
    static char tasks_buf[512];
    static char alerts_buf[512];

    if (xSemaphoreTake(data_mutex, pdMS_TO_TICKS(200))) {

        // Temperature history
        strcpy(history_buf, "[");
        for (int i = 0; i < temp_count; i++) {
            char num[12];
            snprintf(num, sizeof(num), "%.1f", temp_history[i]);
            strcat(history_buf, num);
            if (i < temp_count - 1) strcat(history_buf, ",");
        }
        strcat(history_buf, "]");

        // Task stats
        strcpy(tasks_buf, "[");
        for (int i = 0; i < latest_stats.task_count && i < 8; i++) {
            char t[128];
            snprintf(t, sizeof(t),
                     "{\"name\":\"%s\",\"stack\":%lu,\"cpu\":\"%.1f\"}",
                     latest_stats.tasks[i].name,
                     latest_stats.tasks[i].stack_hwm,
                     latest_stats.tasks[i].cpu_percent);
            strcat(tasks_buf, t);
            if (i < latest_stats.task_count - 1 && i < 7)
                strcat(tasks_buf, ",");
        }
        strcat(tasks_buf, "]");

        // Alert history
        strcpy(alerts_buf, "[");
        for (int i = 0; i < alert_count; i++) {
            char a[128];
            snprintf(a, sizeof(a),
                     "{\"msg\":\"%s\",\"ts\":%lu}",
                     alert_history[i].message,
                     alert_history[i].timestamp);
            strcat(alerts_buf, a);
            if (i < alert_count - 1) strcat(alerts_buf, ",");
        }
        strcat(alerts_buf, "]");

        float current = temp_count > 0
            ? temp_history[(temp_index - 1 + MAX_READINGS) % MAX_READINGS]
            : 0.0f;

        snprintf(json, sizeof(json),
                 "{\"current\":%.1f,\"history\":%s,"
                 "\"heap\":%lu,\"uptime\":%lu,\"task_count\":%d,"
                 "\"tasks\":%s,\"alerts\":%s}",
                 current, history_buf,
                 latest_stats.free_heap,
                 latest_stats.uptime_s,
                 latest_stats.task_count,
                 tasks_buf, alerts_buf);

        xSemaphoreGive(data_mutex);
    } else {
        snprintf(json, sizeof(json), "{\"error\":\"mutex timeout\"}");
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, strlen(json));
    return ESP_OK;
}

void webserver_start(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 8192;  // bigger stack for this handler
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t root = { .uri="/",      .method=HTTP_GET, .handler=dashboard_handler };
        httpd_uri_t data = { .uri="/data",  .method=HTTP_GET, .handler=data_handler };
        httpd_register_uri_handler(server, &root);
        httpd_register_uri_handler(server, &data);
        ESP_LOGI(TAG, "Dashboard: http://172.16.0.20");
    }
}