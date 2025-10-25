#include <stdio.h>
#include <math.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_task_wdt.h" // Include Task Watchdog Timer header

static const char *TAG = "REALTIME";

/* ===================== Configuration ===================== */
#define CORE0              0
#define CORE1              1

// Target frequencies
#define CTRL_HZ            1000   // 1 kHz
#define DAQ_HZ             500    // 500 Hz

// Periods (microseconds)
#define CTRL_PERIOD_US     (1000000 / CTRL_HZ)  // 1000 us
#define DAQ_PERIOD_US      (1000000 / DAQ_HZ)   // 2000 us

// Priorities (must be < configMAX_PRIORITIES = 25)
#define PRIO_CTRL          24
#define PRIO_DAQ           22
#define PRIO_COMM          18
#define PRIO_BG             5

// Stack sizes
#define STK_CTRL           4096
#define STK_DAQ            4096
#define STK_COMM           4096
#define STK_BG             4096

// Reporting interval (milliseconds)
#define REPORT_MS          1000

/* ============= Communication Structures ============ */
typedef struct {
    int64_t t_send_us;      // Time sent (microseconds)
    uint32_t seq;           // Sequence number
    float ctrl_output;      // Control loop output (example)
} ctrl_msg_t;

static QueueHandle_t q_ctrl_to_comm;

/* ============= Frequency/Jitter Measurement Helpers ============= */
typedef struct {
    int64_t prev_tick_us;
    int64_t target_period_us;
    // Simple statistics
    double err_abs_sum_us;
    double err_abs_max_us;
    uint32_t count;
} period_stats_t;

static inline void stats_init(period_stats_t *s, int64_t period_us) {
    memset(s, 0, sizeof(*s));
    s->target_period_us = period_us;
    s->prev_tick_us = 0;
    s->err_abs_sum_us = 0.0;
    s->err_abs_max_us = 0.0;
    s->count = 0;
}

static inline void stats_update(period_stats_t *s, int64_t now_us) {
    if (s->prev_tick_us == 0) {
        s->prev_tick_us = now_us;
        return;
    }
    int64_t dt = now_us - s->prev_tick_us;
    s->prev_tick_us = now_us;

    double err = (double)dt - (double)s->target_period_us;   // Microseconds
    double aerr = fabs(err);
    s->err_abs_sum_us += aerr;
    if (aerr > s->err_abs_max_us) s->err_abs_max_us = aerr;
    s->count++;
}

static inline void stats_report_and_clear(const char *tag_name, const char *label, const period_stats_t *s) {
    if (s->count == 0) return;
    double avg_abs_err = s->err_abs_sum_us / (double)s->count;   // Microseconds
    double jitter_pct  = (avg_abs_err / (double)s->target_period_us) * 100.0; // Percentage
    double max_jitter_pct = (s->err_abs_max_us / (double)s->target_period_us) * 100.0;

    double hz = 1e6 / (double)s->target_period_us;
    ESP_LOGI(tag_name, "%s: %.1f Hz (jitter avg: ±%.2f%%, max: ±%.2f%%)",
             label, hz, jitter_pct, max_jitter_pct);
}

/* ======= vTaskDelayUntil แบบความละเอียด us บน esp_timer ======= */
/* ใช้ clock ความละเอียดสูงเพื่อคุม 1kHz/500Hz แบบเนียน ไม่ busy-wait ยาว */
static inline void delay_until_us(int64_t *next_deadline_us, int64_t period_us)
{
    int64_t now = esp_timer_get_time();
    if (*next_deadline_us == 0) {
        *next_deadline_us = now + period_us;
    } else {
        *next_deadline_us += period_us;
    }
    int64_t wait_us = *next_deadline_us - now;
    if (wait_us <= 0) {
        // If behind schedule, catch up without busy-waiting
        return;
    }
    // Convert to ticks and use vTaskDelay for as much as possible, then compensate with short us delay
    if (wait_us >= 1000) {
        TickType_t dly = pdMS_TO_TICKS((uint32_t)(wait_us / 1000));
        if (dly > 0) vTaskDelay(dly);
    }
    // Compensate remaining time with a short delay
    int64_t remain = *next_deadline_us - esp_timer_get_time();
    if (remain > 0 && remain < 1000) {
        taskYIELD();
    }
}

/* ===================== Dummy workloads ===================== */
static float do_control_compute(uint32_t k)
{
    // Dummy computation workload
    volatile float acc = 0.f;
    for (int i = 0; i < 200; ++i) {  // Avoid excessive load that could affect timing
        acc += sqrtf((float)(i + 1)) * 0.001f;
    }
    return acc + (k & 0x7) * 0.01f;
}

static void do_daq_read(float *v1, float *v2)
{
    // Simulate ADC/sensor reading
    static float t = 0.f;
    t += 0.05f;
    *v1 = 1.23f + 0.1f * sinf(t);
    *v2 = 3.45f + 0.1f * cosf(t);
}

static void do_comm_io(void)
{
    // Simulate non-blocking I/O (e.g., MQTT/Socket)
    vTaskDelay(pdMS_TO_TICKS(5));
}

static void do_background_work(void)
{
    // Light background work
    vTaskDelay(pdMS_TO_TICKS(50));
}

/* ====================== Tasks ======================= */

// Control 1 kHz @ Core0
static void control_task_core0(void *arg)
{
    ESP_LOGI(TAG, "Starting Control Task on Core %d", xPortGetCoreID());

    period_stats_t stats;
    stats_init(&stats, CTRL_PERIOD_US);

    int64_t next_deadline_us = 0;
    int64_t last_report_time = esp_timer_get_time();
    uint32_t sequence_number = 0;

    // Register this task with the Task Watchdog Timer
    esp_task_wdt_add(NULL);

    while (1) {
        int64_t start_time = esp_timer_get_time();

        // Execute control logic
        float control_output = do_control_compute(sequence_number);

        // Send data to the communication task
        ctrl_msg_t message = {
            .t_send_us = start_time,
            .seq = sequence_number++,
            .ctrl_output = control_output
        };
        if (xQueueSend(q_ctrl_to_comm, &message, 0) != pdPASS) {
            ESP_LOGW(TAG, "Control Task: Queue send failed");
        }

        // Update timing statistics
        int64_t end_time = esp_timer_get_time();
        stats_update(&stats, end_time);

        // Log statistics every second
        if ((end_time - last_report_time) >= (REPORT_MS * 1000)) {
            stats_report_and_clear(TAG, "Control Loop Stats", &stats);
            stats_init(&stats, CTRL_PERIOD_US);
            last_report_time = end_time;
        }

        // Reset the watchdog timer
        esp_task_wdt_reset();

        // Maintain the task period
        delay_until_us(&next_deadline_us, CTRL_PERIOD_US);
    }

    // Unregister this task from the Task Watchdog Timer
    esp_task_wdt_delete(NULL);
}

// Data Acquisition 500 Hz @ Core0
static void daq_task_core0(void *arg)
{
    ESP_LOGI(TAG, "DAQ task start on Core %d", xPortGetCoreID());

    period_stats_t stats;
    stats_init(&stats, DAQ_PERIOD_US);

    int64_t next_deadline_us = 0;
    int64_t last_report = esp_timer_get_time();

    while (1) {
        float a, b;
        do_daq_read(&a, &b);

        int64_t now = esp_timer_get_time();
        stats_update(&stats, now);

        if ((now - last_report) >= (REPORT_MS * 1000)) {
            stats_report_and_clear(TAG, "Data acquisition", &stats);
            stats_init(&stats, DAQ_PERIOD_US);
            last_report = now;
        }

        delay_until_us(&next_deadline_us, DAQ_PERIOD_US);
    }
}

// Communication @ Core1
static void comm_task_core1(void *arg)
{
    ESP_LOGI(TAG, "Starting Communication Task on Core %d", xPortGetCoreID());

    uint32_t received_count = 0;
    int64_t last_report_time = esp_timer_get_time();
    double total_latency_ms = 0.0, max_latency_ms = 0.0;

    // Register this task with the Task Watchdog Timer
    esp_task_wdt_add(NULL);

    while (1) {
        ctrl_msg_t received_message;
        if (xQueueReceive(q_ctrl_to_comm, &received_message, pdMS_TO_TICKS(10)) == pdTRUE) {
            int64_t current_time = esp_timer_get_time();
            double latency_ms = (double)(current_time - received_message.t_send_us) / 1000.0;
            total_latency_ms += latency_ms;
            if (latency_ms > max_latency_ms) max_latency_ms = latency_ms;
            received_count++;
        }

        // Perform communication I/O
        do_comm_io();

        // Log latency statistics every second
        int64_t now = esp_timer_get_time();
        if ((now - last_report_time) >= (REPORT_MS * 1000)) {
            if (received_count > 0) {
                double average_latency_ms = total_latency_ms / (double)received_count;
                ESP_LOGI(TAG, "Comm Latency: Avg = %.2f ms, Max = %.2f ms",
                         average_latency_ms, max_latency_ms);
            } else {
                ESP_LOGI(TAG, "Comm Latency: No messages received");
            }
            received_count = 0;
            total_latency_ms = 0.0;
            max_latency_ms = 0.0;
            last_report_time = now;
        }

        // Reset the watchdog timer
        esp_task_wdt_reset();
    }

    // Unregister this task from the Task Watchdog Timer
    esp_task_wdt_delete(NULL);
}

// Background (no affinity)
static void background_task(void *arg)
{
    ESP_LOGI(TAG, "Background task on Core %d", xPortGetCoreID());
    while (1) {
        do_background_work();
        // Example log message at intervals
        static uint32_t n = 0;
        if ((++n % 20) == 0) {
            ESP_LOGI(TAG, "BG alive. Free heap ~ %d bytes", (int)esp_get_free_heap_size());
        }
    }
}

/* ===================== app_main ===================== */
void app_main(void)
{
    ESP_LOGI(TAG, "ESP32 Core-Pinned Real-Time Demo; Main on Core %d", xPortGetCoreID());

    // Communication queue from Control to Comm
    q_ctrl_to_comm = xQueueCreate(32, sizeof(ctrl_msg_t));
    configASSERT(q_ctrl_to_comm != NULL);

    // Create tasks with priority levels under 0..24
    BaseType_t ok;

    ok = xTaskCreatePinnedToCore(control_task_core0, "Ctrl_1kHz", STK_CTRL, NULL, PRIO_CTRL, NULL, CORE0);
    configASSERT(ok == pdPASS);

    ok = xTaskCreatePinnedToCore(daq_task_core0, "DAQ_500Hz", STK_DAQ, NULL, PRIO_DAQ, NULL, CORE0);
    configASSERT(ok == pdPASS);

    ok = xTaskCreatePinnedToCore(comm_task_core1, "Comm", STK_COMM, NULL, PRIO_COMM, NULL, CORE1);
    configASSERT(ok == pdPASS);

    ok = xTaskCreate(background_task, "BG", STK_BG, NULL, PRIO_BG, NULL);
    configASSERT(ok == pdPASS);
}