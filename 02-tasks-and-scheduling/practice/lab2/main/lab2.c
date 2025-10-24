#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define LED_RUNNING GPIO_NUM_2      // แสดง Running state
#define LED_READY GPIO_NUM_4        // แสดง Ready state
#define LED_BLOCKED GPIO_NUM_5      // แสดง Blocked state
#define LED_SUSPENDED GPIO_NUM_18   // แสดง Suspended state

#define BUTTON1_PIN GPIO_NUM_0      // Control button
#define BUTTON2_PIN GPIO_NUM_35     // State change button

static const char *TAG = "TASK_STATES";

// Task handles สำหรับการควบคุม
TaskHandle_t state_demo_task_handle = NULL;
TaskHandle_t control_task_handle = NULL;

// Semaphore สำหรับ blocking demonstration
SemaphoreHandle_t demo_semaphore = NULL;

// State names สำหรับแสดงผล
const char* state_names[] = {
    "Running",      // 0
    "Ready",        // 1  
    "Blocked",      // 2
    "Suspended",    // 3
    "Deleted",      // 4
    "Invalid"       // 5
};

// Get state name string
const char* get_state_name(eTaskState state)
{
    if (state <= eDeleted) {
        return state_names[state];
    }
    return state_names[5]; // Invalid
}

void monitor_task_states(void);
void count_state_change(eTaskState old_state, eTaskState new_state);
void update_state_display(eTaskState current_state);

// Task สำหรับสาธิต states ต่างๆ
void state_demo_task(void *pvParameters)
{
    ESP_LOGI(TAG, "State Demo Task started");
    
    int cycle = 0;
    
    while (1) {
        cycle++;
        ESP_LOGI(TAG, "=== Cycle %d ===", cycle);
        
        // State 1: Running
        ESP_LOGI(TAG, "Task is RUNNING");
        gpio_set_level(LED_RUNNING, 1);
        gpio_set_level(LED_READY, 0);
        gpio_set_level(LED_BLOCKED, 0);
        gpio_set_level(LED_SUSPENDED, 0);
        
        // ทำงานหนักๆ เพื่อแสดง Running state
        for (int i = 0; i < 1000000; i++) {
            // Removed unused variable
        }
        
        // State 2: Ready (เมื่อมี task อื่นที่ priority เท่ากัน)
        ESP_LOGI(TAG, "Task will be READY (yielding to other tasks)");
        gpio_set_level(LED_RUNNING, 0);
        gpio_set_level(LED_READY, 1);
        
        taskYIELD(); // ให้ task อื่นที่มี priority เท่ากันทำงาน
        vTaskDelay(pdMS_TO_TICKS(100)); // กลับไปเป็น Ready state สั้นๆ
        
        // State 3: Blocked (รอ semaphore)
        ESP_LOGI(TAG, "Task will be BLOCKED (waiting for semaphore)");
        gpio_set_level(LED_READY, 0);
        gpio_set_level(LED_BLOCKED, 1);
        
        // รอ semaphore (จะ block อยู่ที่นี่)
        if (xSemaphoreTake(demo_semaphore, pdMS_TO_TICKS(2000)) == pdTRUE) {
            ESP_LOGI(TAG, "Got semaphore! Task is RUNNING again");
            gpio_set_level(LED_BLOCKED, 0);
            gpio_set_level(LED_RUNNING, 1);
            
            // ทำงานสั้นๆ
            vTaskDelay(pdMS_TO_TICKS(500));
        } else {
            ESP_LOGI(TAG, "Semaphore timeout! Continuing...");
            gpio_set_level(LED_BLOCKED, 0);
        }
        
        // State 4: Normal delay (ซึ่งจริงๆ แล้วเป็น Blocked state)
        ESP_LOGI(TAG, "Task is BLOCKED (in vTaskDelay)");
        gpio_set_level(LED_RUNNING, 0);
        gpio_set_level(LED_BLOCKED, 1);
        
        vTaskDelay(pdMS_TO_TICKS(1000)); // Blocked ใน delay
        
        gpio_set_level(LED_BLOCKED, 0);
        
        // ตรวจสอบว่าจะถูก suspend หรือไม่
        // (การ suspend จะทำโดย control task)
    }
}

// Task สำหรับแสดง task ที่มี priority เท่ากัน (เพื่อสาธิต Ready state)
void ready_state_demo_task(void *pvParameters)
{
    while (1) {
        ESP_LOGI(TAG, "Ready state demo task running");
        
        // ทำงานเล็กน้อย
        for (int i = 0; i < 100000; i++) {
            // Removed unused variable
        }
        
        vTaskDelay(pdMS_TO_TICKS(150));
    }
}

// Declared external_delete_handle globally
TaskHandle_t external_delete_handle = NULL;

// Control task สำหรับควบคุม states
void control_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Control Task started");

    bool suspended = false;
    int control_cycle = 0;
    static bool external_deleted = false;

    eTaskState previous_state = eTaskGetState(state_demo_task_handle);

    while (1) {
        control_cycle++;

        // Check button 1 - Suspend/Resume
        if (gpio_get_level(BUTTON1_PIN) == 0) {
            vTaskDelay(pdMS_TO_TICKS(50)); // Debounce

            if (!suspended) {
                ESP_LOGW(TAG, "=== SUSPENDING State Demo Task ===");
                eTaskState old_state = eTaskGetState(state_demo_task_handle);
                vTaskSuspend(state_demo_task_handle);
                eTaskState new_state = eTaskGetState(state_demo_task_handle);
                count_state_change(old_state, new_state);
                update_state_display(new_state);

                gpio_set_level(LED_SUSPENDED, 1);
                gpio_set_level(LED_RUNNING, 0);
                gpio_set_level(LED_READY, 0);
                gpio_set_level(LED_BLOCKED, 0);
                suspended = true;
            } else {
                ESP_LOGW(TAG, "=== RESUMING State Demo Task ===");
                eTaskState old_state = eTaskGetState(state_demo_task_handle);
                vTaskResume(state_demo_task_handle);
                eTaskState new_state = eTaskGetState(state_demo_task_handle);
                count_state_change(old_state, new_state);
                update_state_display(new_state);

                gpio_set_level(LED_SUSPENDED, 0);
                suspended = false;
            }

            while (gpio_get_level(BUTTON1_PIN) == 0) {
                vTaskDelay(pdMS_TO_TICKS(10));
            }
        }

        // Check button 2 - Give semaphore
        if (gpio_get_level(BUTTON2_PIN) == 0) {
            vTaskDelay(pdMS_TO_TICKS(50)); // Debounce

            ESP_LOGW(TAG, "=== GIVING SEMAPHORE ===");
            xSemaphoreGive(demo_semaphore);

            while (gpio_get_level(BUTTON2_PIN) == 0) {
                vTaskDelay(pdMS_TO_TICKS(10));
            }
        }

        // แสดงสถานะ task ทุก 3 วินาที
        if (control_cycle % 30 == 0) {
            ESP_LOGI(TAG, "=== TASK STATUS REPORT ===");

            eTaskState current_state = eTaskGetState(state_demo_task_handle);
            count_state_change(previous_state, current_state);
            update_state_display(current_state);
            previous_state = current_state;

            ESP_LOGI(TAG, "State Demo Task: %s", get_state_name(current_state));

            // แสดงข้อมูลเพิ่มเติม
            UBaseType_t priority = uxTaskPriorityGet(state_demo_task_handle);
            ESP_LOGI(TAG, "Priority: %d", priority);
            
            // Stack usage
            UBaseType_t stack_remaining = uxTaskGetStackHighWaterMark(state_demo_task_handle);
            ESP_LOGI(TAG, "Stack remaining: %d bytes", stack_remaining * sizeof(StackType_t));
        }
        
        // ลบ external_delete_task หลังจาก 15 วินาที
        if (control_cycle == 150 && !external_deleted) {
            ESP_LOGW(TAG, "Deleting external task");
            vTaskDelete(external_delete_handle);
            external_deleted = true;
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// Task สำหรับแสดงสถิติระบบ
void system_monitor_task(void *pvParameters)
{
    ESP_LOGI(TAG, "System Monitor started");
    
    char *task_list_buffer = malloc(1024);
    char *stats_buffer = malloc(1024);
    
    if (!task_list_buffer || !stats_buffer) {
        ESP_LOGE(TAG, "Failed to allocate buffers");
        vTaskDelete(NULL);
        return;
    }
    
    while (1) {
        ESP_LOGI(TAG, "\n=== SYSTEM MONITOR ===");
        
        // Task list
        vTaskList(task_list_buffer);
        ESP_LOGI(TAG, "Task List:");
        ESP_LOGI(TAG, "Name\t\tState\tPrio\tStack\tNum");
        ESP_LOGI(TAG, "%s", task_list_buffer);
        
        // Runtime statistics
        vTaskGetRunTimeStats(stats_buffer);
        ESP_LOGI(TAG, "\nRuntime Stats:");
        ESP_LOGI(TAG, "Task\t\tAbs Time\t%%Time");
        ESP_LOGI(TAG, "%s", stats_buffer);
        
        vTaskDelay(pdMS_TO_TICKS(5000)); // ทุก 5 วินาที
    }
    
    free(task_list_buffer);
    free(stats_buffer);
}

// Task ที่สาธิต self-deletion
void self_deleting_task(void *pvParameters)
{
    int *lifetime = (int *)pvParameters;

    ESP_LOGI(TAG, "Self-deleting task will live for %d seconds", *lifetime);

    for (int i = *lifetime; i > 0; i--) {
        ESP_LOGI(TAG, "Self-deleting task countdown: %d", i);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    ESP_LOGI(TAG, "Self-deleting task going to DELETED state");
    vTaskDelete(NULL); // DELETED state
}

// Task ที่จะถูก delete จากภายนอก
void external_delete_task(void *pvParameters)
{
    int count = 0;

    while (1) {
        ESP_LOGI(TAG, "External delete task running: %d", count++);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    // จุดนี้จะไม่ถูกเรียกเพราะ task จะถูก delete จากภายนอก
}

// Added function to monitor task states
void monitor_task_states(void)
{
    ESP_LOGI(TAG, "=== DETAILED TASK STATE MONITOR ===");

    // List of tasks to monitor
    TaskHandle_t tasks[] = {
        state_demo_task_handle,
        control_task_handle,
        // Add other task handles here
    };

    const char* task_names[] = {
        "StateDemo",
        "Control",
    };

    int num_tasks = sizeof(tasks) / sizeof(tasks[0]);

    for (int i = 0; i < num_tasks; i++) {
        if (tasks[i] != NULL) {
            eTaskState state = eTaskGetState(tasks[i]);
            UBaseType_t priority = uxTaskPriorityGet(tasks[i]);
            UBaseType_t stack_remaining = uxTaskGetStackHighWaterMark(tasks[i]);

            ESP_LOGI(TAG, "%s: State=%s, Priority=%d, Stack=%d bytes", 
                     task_names[i], 
                     get_state_name(state), 
                     priority, 
                     stack_remaining * sizeof(StackType_t));
        }
    }
}

// Added state transition counter
volatile uint32_t state_changes[5] = {0}; // For each state

void count_state_change(eTaskState old_state, eTaskState new_state)
{
    if (old_state != new_state && new_state <= eDeleted) {
        state_changes[new_state]++;
        ESP_LOGI(TAG, "State change: %s -> %s (Count: %d)", 
                 get_state_name(old_state), 
                 get_state_name(new_state), 
                 state_changes[new_state]);
    }
}

// Added function to update state display
void update_state_display(eTaskState current_state)
{
    // Turn off all LEDs first
    gpio_set_level(LED_RUNNING, 0);
    gpio_set_level(LED_READY, 0);
    gpio_set_level(LED_BLOCKED, 0);
    gpio_set_level(LED_SUSPENDED, 0);

    // Turn on appropriate LED
    switch (current_state) {
        case eRunning:
            gpio_set_level(LED_RUNNING, 1);
            break;
        case eReady:
            gpio_set_level(LED_READY, 1);
            break;
        case eBlocked:
            gpio_set_level(LED_BLOCKED, 1);
            break;
        case eSuspended:
            gpio_set_level(LED_SUSPENDED, 1);
            break;
        default:
            // Blink all LEDs for unknown state
            for (int i = 0; i < 3; i++) {
                gpio_set_level(LED_RUNNING, 1);
                gpio_set_level(LED_READY, 1);
                gpio_set_level(LED_BLOCKED, 1);
                gpio_set_level(LED_SUSPENDED, 1);
                vTaskDelay(pdMS_TO_TICKS(100));
                gpio_set_level(LED_RUNNING, 0);
                gpio_set_level(LED_READY, 0);
                gpio_set_level(LED_BLOCKED, 0);
                gpio_set_level(LED_SUSPENDED, 0);
                vTaskDelay(pdMS_TO_TICKS(100));
            }
            break;
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== FreeRTOS Task States Demo ===");
    
    // GPIO Configuration
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << LED_RUNNING) | (1ULL << LED_READY) | 
                        (1ULL << LED_BLOCKED) | (1ULL << LED_SUSPENDED),
        .pull_down_en = 0,
        .pull_up_en = 0,
    };
    gpio_config(&io_conf);

    // Button configuration
    gpio_config_t button_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << BUTTON1_PIN) | (1ULL << BUTTON2_PIN),
        .pull_up_en = 1,
        .pull_down_en = 0,
    };
    gpio_config(&button_conf);

    // สร้าง semaphore
    demo_semaphore = xSemaphoreCreateBinary();
    if (demo_semaphore == NULL) {
        ESP_LOGE(TAG, "Failed to create semaphore");
        return;
    }

    ESP_LOGI(TAG, "LED Indicators:");
    ESP_LOGI(TAG, "GPIO2 = Running, GPIO4 = Ready");
    ESP_LOGI(TAG, "GPIO5 = Blocked, GPIO18 = Suspended");
    ESP_LOGI(TAG, "Button Controls:");
    ESP_LOGI(TAG, "GPIO0 = Suspend/Resume, GPIO35 = Give Semaphore");

    // สร้าง tasks
    xTaskCreate(state_demo_task, "StateDemo", 4096, NULL, 3, &state_demo_task_handle);
    xTaskCreate(ready_state_demo_task, "ReadyDemo", 2048, NULL, 3, NULL); // Priority เดียวกัน
    xTaskCreate(control_task, "Control", 3072, NULL, 4, &control_task_handle);
    xTaskCreate(system_monitor_task, "Monitor", 4096, NULL, 1, NULL);

    static int self_delete_time = 10;

    xTaskCreate(self_deleting_task, "SelfDelete", 2048, &self_delete_time, 2, NULL);
    xTaskCreate(external_delete_task, "ExtDelete", 2048, NULL, 2, &external_delete_handle);

    ESP_LOGI(TAG, "All tasks created. Monitoring task states...");
}