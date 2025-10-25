#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_task_wdt.h"

static const char *TAG = "PERFORMANCE_OPTIMIZATION";

// Task handles
static TaskHandle_t monitor_task_handle = NULL;

// Provide a timer for runtime statistics
uint32_t vPortGetRunTimeStats(void) {
    return (uint32_t)(esp_timer_get_time() / 1000); // Convert microseconds to milliseconds
}

// Performance monitoring task
void monitor_task(void *arg) {
    while (1) {
        // Log runtime statistics
        char runtime_stats[1024];
        vTaskGetRunTimeStats(runtime_stats);
        ESP_LOGI(TAG, "Runtime Stats:\n%s", runtime_stats);

        // Log heap memory usage
        ESP_LOGI(TAG, "Free Heap: %d bytes", esp_get_free_heap_size());

        vTaskDelay(pdMS_TO_TICKS(5000)); // Log every 5 seconds
    }
}

// Example task to simulate workload
void example_task(void *arg) {
    while (1) {
        ESP_LOGI(TAG, "Example task running");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "Starting Performance Optimization Example");

    // Initialize watchdog timer with correct configuration
    esp_task_wdt_config_t wdt_config = {
        .timeout_ms = 10000, // 10-second timeout
        .idle_core_mask = 0, // Monitor all cores
        .trigger_panic = true // Trigger panic on timeout
    };
    // Check if the Task Watchdog Timer (TWDT) is already initialized
    esp_err_t wdt_init_result = esp_task_wdt_init(&wdt_config);
    if (wdt_init_result == ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "Task Watchdog Timer already initialized. Skipping initialization.");
    } else {
        ESP_ERROR_CHECK(wdt_init_result);
    }
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL)); // Add current task to watchdog

    // Create tasks
    xTaskCreate(monitor_task, "Monitor_Task", 4096, NULL, 5, &monitor_task_handle);
    xTaskCreate(example_task, "Example_Task", 2048, NULL, 10, NULL);

    // Simulate workload
    while (1) {
        ESP_LOGI(TAG, "Main task running");
        esp_task_wdt_reset(); // Reset watchdog timer
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
