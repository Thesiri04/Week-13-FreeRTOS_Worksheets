#include <stdio.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_system.h"

static const char *TAG = "DUAL_CORE";

// Queue handle for inter-core communication
static QueueHandle_t intercore_queue;

// Task running on Core 0: Compute-intensive task
void compute_task(void *pvParameters) {
    ESP_LOGI(TAG, "Compute task running on Core 0");
    uint32_t counter = 0;

    while (1) {
        counter++;
        if (counter % 1000000 == 0) {
            ESP_LOGI(TAG, "Core 0: Counter = %u", counter);
            xQueueSend(intercore_queue, &counter, portMAX_DELAY);
        }
    }
}

// Task running on Core 1: I/O and communication task
void io_task(void *pvParameters) {
    ESP_LOGI(TAG, "I/O task running on Core 1");
    uint32_t received_counter;

    while (1) {
        if (xQueueReceive(intercore_queue, &received_counter, portMAX_DELAY)) {
            ESP_LOGI(TAG, "Core 1: Received counter = %u", received_counter);
        }
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "Starting Dual-Core Task Distribution Example");

    // Create the queue for inter-core communication
    intercore_queue = xQueueCreate(10, sizeof(uint32_t));
    if (intercore_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create queue");
        return;
    }

    // Create tasks pinned to specific cores
    xTaskCreatePinnedToCore(compute_task, "ComputeTask", 2048, NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(io_task, "IOTask", 2048, NULL, 5, NULL, 1);

    ESP_LOGI(TAG, "Tasks created successfully");
}
