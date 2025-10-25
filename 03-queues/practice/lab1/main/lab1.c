#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "driver/gpio.h"

static const char *TAG = "QUEUE_LAB";

// LED pins
#define LED_SENDER GPIO_NUM_2
#define LED_RECEIVER GPIO_NUM_4

// Queue handle
QueueHandle_t xQueue;

// Data structure for queue messages
typedef struct {
    int id;
    char message[50];
    uint32_t timestamp;
} queue_message_t;

// Sender task
void sender_task(void *pvParameters) {
    queue_message_t message;
    int counter = 0;
    
    ESP_LOGI(TAG, "Sender task started");
    
    while (1) {
        // Prepare message
        message.id = counter++;
        snprintf(message.message, sizeof(message.message), 
                "Hello from sender #%d", message.id);
        message.timestamp = xTaskGetTickCount();
        
        // Send message to queue
        BaseType_t xStatus = xQueueSend(xQueue, &message, pdMS_TO_TICKS(1000));
        
        if (xStatus == pdPASS) {
            ESP_LOGI(TAG, "Sent: ID=%d, MSG=%s, Time=%lu", 
                    message.id, message.message, message.timestamp);
            
            // Blink sender LED
            gpio_set_level(LED_SENDER, 1);
            vTaskDelay(pdMS_TO_TICKS(100));
            gpio_set_level(LED_SENDER, 0);
        } else {
            ESP_LOGW(TAG, "Failed to send message (queue full?)");
        }
        
        vTaskDelay(pdMS_TO_TICKS(500)); // Send every 500 milliseconds
    }
}

// Receiver task
void receiver_task(void *pvParameters) {
    queue_message_t received_message;
    
    ESP_LOGI(TAG, "Receiver task started");
    
    while (1) {
        // Wait for message from queue
        BaseType_t xStatus = xQueueReceive(xQueue, &received_message, 
                                          pdMS_TO_TICKS(5000));
        
        if (xStatus == pdPASS) {
            ESP_LOGI(TAG, "Received: ID=%d, MSG=%s, Time=%lu", 
                    received_message.id, received_message.message, 
                    received_message.timestamp);
            
            // Blink receiver LED
            gpio_set_level(LED_RECEIVER, 1);
            vTaskDelay(pdMS_TO_TICKS(200));
            gpio_set_level(LED_RECEIVER, 0);
            
            // Process time (simulate work)
            vTaskDelay(pdMS_TO_TICKS(100));
        } else {
            ESP_LOGW(TAG, "No message received within timeout");
        }
    }
}

// Queue monitoring task
void queue_monitor_task(void *pvParameters) {
    UBaseType_t uxMessagesWaiting;
    UBaseType_t uxSpacesAvailable;
    
    ESP_LOGI(TAG, "Queue monitor task started");
    
    while (1) {
        uxMessagesWaiting = uxQueueMessagesWaiting(xQueue);
        uxSpacesAvailable = uxQueueSpacesAvailable(xQueue);
        
        ESP_LOGI(TAG, "Queue Status - Messages: %d, Free spaces: %d", 
                uxMessagesWaiting, uxSpacesAvailable);
        
        // Show queue fullness on console
        printf("Queue: [");
        for (int i = 0; i < 5; i++) {
            if (i < uxMessagesWaiting) {
                printf("■");
            } else {
                printf("□");
            }
        }
        printf("]\n");
        
        vTaskDelay(pdMS_TO_TICKS(3000)); // Monitor every 3 seconds
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "Basic Queue Operations Lab Starting...");
    
    // Configure LED pins
    gpio_set_direction(LED_SENDER, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_RECEIVER, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_SENDER, 0);
    gpio_set_level(LED_RECEIVER, 0);
    
    // Create queue (can hold 5 messages)
    xQueue = xQueueCreate(5, sizeof(queue_message_t));
    
    if (xQueue != NULL) {
        ESP_LOGI(TAG, "Queue created successfully (size: 5 messages)");
        
        // Create tasks
        xTaskCreate(sender_task, "Sender", 2048, NULL, 2, NULL);
        xTaskCreate(receiver_task, "Receiver", 2048, NULL, 1, NULL);
        xTaskCreate(queue_monitor_task, "Monitor", 2048, NULL, 1, NULL);
        
        ESP_LOGI(TAG, "All tasks created. Starting scheduler...");
    } else {
        ESP_LOGE(TAG, "Failed to create queue!");
    }
}