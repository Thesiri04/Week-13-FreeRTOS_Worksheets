#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "driver/gptimer.h"
#include "driver/spi_master.h"
#include "driver/i2c.h"

static const char *TAG = "PERIPHERAL_INTEGRATION";

// GPIO configuration
#define GPIO_INPUT_PIN  GPIO_NUM_4
#define GPIO_OUTPUT_PIN GPIO_NUM_2
#define ESP_INTR_FLAG_DEFAULT 0

// Timer configuration
#define TIMER_INTERVAL_SEC (1.0) // 1 second

// DMA channel for SPI
#define DMA_CHAN    1

// I2C configuration
#define I2C_MASTER_SCL_IO 22
#define I2C_MASTER_SDA_IO 21
#define I2C_MASTER_FREQ_HZ 100000

// Define a mock timer_task
void timer_task(void* arg) {
    while (1) {
        ESP_LOGI(TAG, "[SIMULATION] Timer task triggered");
        vTaskDelay(pdMS_TO_TICKS(1000)); // Simulate timer event every 1 second
    }
}

// Simulate SPI and I2C transactions
void spi_task(void* arg) {
    while (1) {
        ESP_LOGI(TAG, "[SIMULATION] Performing mock SPI transaction");
        vTaskDelay(pdMS_TO_TICKS(500)); // Simulate delay
    }
}

void i2c_task(void* arg) {
    while (1) {
        ESP_LOGI(TAG, "[SIMULATION] Performing mock I2C transaction");
        vTaskDelay(pdMS_TO_TICKS(1000)); // Simulate delay
    }
}

// Simulate GPIO events
void gpio_task(void* arg) {
    while (1) {
        ESP_LOGI(TAG, "[SIMULATION] Mock GPIO interrupt on pin %d", GPIO_INPUT_PIN);
        gpio_set_level(GPIO_OUTPUT_PIN, !gpio_get_level(GPIO_OUTPUT_PIN));
        vTaskDelay(pdMS_TO_TICKS(2000)); // Simulate GPIO event every 2 seconds
    }
}

// Simulate WiFi events
static void simulate_wifi_events(void* arg) {
    while (1) {
        ESP_LOGI(TAG, "[SIMULATION] WiFi connected");
        vTaskDelay(pdMS_TO_TICKS(5000)); // Simulate connection for 5 seconds

        ESP_LOGI(TAG, "[SIMULATION] WiFi disconnected");
        vTaskDelay(pdMS_TO_TICKS(3000)); // Simulate disconnection for 3 seconds
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "Starting Peripheral Integration Simulation");

    // Create tasks for simulation
    xTaskCreate(gpio_task, "GPIO_Task", 2048, NULL, 10, NULL);
    xTaskCreate(timer_task, "Timer_Task", 2048, NULL, 10, NULL);
    xTaskCreate(spi_task, "SPI_Task", 2048, NULL, 10, NULL);
    xTaskCreate(i2c_task, "I2C_Task", 2048, NULL, 10, NULL);
    xTaskCreate(simulate_wifi_events, "WiFi_Simulation", 2048, NULL, 10, NULL);
}
