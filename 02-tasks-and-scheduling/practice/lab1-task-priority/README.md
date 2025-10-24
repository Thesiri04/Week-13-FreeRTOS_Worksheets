# Lab 1: Task Priority และ Scheduling

## วัตถุประสงค์
ทำความเข้าใจ Priority-based Preemptive Scheduling และการทำงานของ FreeRTOS Scheduler

## เวลาที่ใช้
45 นาที

## อุปกรณ์ที่ต้องใช้
- ESP32 Development Board
- LED 3 ดวง
- Push Button 1 ตัว

## ขั้นตอนการทดลอง

### Step 1: Basic Priority Demonstration (20 นาที)

สร้างโปรเจกต์ใหม่และเขียนโปรแกรม:

```c
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"

#define LED_HIGH_PIN GPIO_NUM_2    // สำหรับ High Priority Task
#define LED_MED_PIN GPIO_NUM_4     // สำหรับ Medium Priority Task
#define LED_LOW_PIN GPIO_NUM_5     // สำหรับ Low Priority Task
#define BUTTON_PIN GPIO_NUM_0      // Trigger button

static const char *TAG = "PRIORITY_DEMO";

// Global variables for statistics
volatile uint32_t high_task_count = 0;
volatile uint32_t med_task_count = 0;
volatile uint32_t low_task_count = 0;
volatile bool priority_test_running = false;

// High Priority Task (Priority 5)
void high_priority_task(void *pvParameters)
{
    ESP_LOGI(TAG, "High Priority Task started (Priority 5)");
    
    while (1) {
        if (priority_test_running) {
            high_task_count++;
            ESP_LOGI(TAG, "HIGH PRIORITY RUNNING (%d)", high_task_count);
            
            gpio_set_level(LED_HIGH_PIN, 1);
            
            // Simulate critical work
            for (int i = 0; i < 100000; i++) {
                volatile int dummy = i * 2;
            }
            
            gpio_set_level(LED_HIGH_PIN, 0);
            
            // High priority task runs briefly then sleeps
            vTaskDelay(pdMS_TO_TICKS(200));
        } else {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

// Medium Priority Task (Priority 3)
void medium_priority_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Medium Priority Task started (Priority 3)");
    
    while (1) {
        if (priority_test_running) {
            med_task_count++;
            ESP_LOGI(TAG, "Medium priority running (%d)", med_task_count);
            
            gpio_set_level(LED_MED_PIN, 1);
            
            // Simulate moderate work
            for (int i = 0; i < 200000; i++) {
                volatile int dummy = i + 100;
            }
            
            gpio_set_level(LED_MED_PIN, 0);
            
            vTaskDelay(pdMS_TO_TICKS(300));
        } else {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

// Low Priority Task (Priority 1)
void low_priority_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Low Priority Task started (Priority 1)");
    
    while (1) {
        if (priority_test_running) {
            low_task_count++;
            ESP_LOGI(TAG, "Low priority running (%d)", low_task_count);
            
            gpio_set_level(LED_LOW_PIN, 1);
            
            // Simulate background work
            for (int i = 0; i < 500000; i++) {
                volatile int dummy = i - 50;
                
                // Yield occasionally to prevent watchdog
                if (i % 100000 == 0) {
                    vTaskDelay(1);
                }
            }
            
            gpio_set_level(LED_LOW_PIN, 0);
            
            vTaskDelay(pdMS_TO_TICKS(500));
        } else {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

// Control Task - starts/stops the test
void control_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Control Task started");
    
    while (1) {
        // Check button press
        if (gpio_get_level(BUTTON_PIN) == 0) {
            if (!priority_test_running) {
                ESP_LOGW(TAG, "=== STARTING PRIORITY TEST ===");
                high_task_count = 0;
                med_task_count = 0;
                low_task_count = 0;
                priority_test_running = true;
                
                // Run test for 10 seconds
                vTaskDelay(pdMS_TO_TICKS(10000));
                
                priority_test_running = false;
                
                ESP_LOGW(TAG, "=== PRIORITY TEST RESULTS ===");
                ESP_LOGI(TAG, "High Priority Task runs: %d", high_task_count);
                ESP_LOGI(TAG, "Medium Priority Task runs: %d", med_task_count);
                ESP_LOGI(TAG, "Low Priority Task runs: %d", low_task_count);
                
                // Analysis
                uint32_t total_runs = high_task_count + med_task_count + low_task_count;
                if (total_runs > 0) {
                    ESP_LOGI(TAG, "High priority percentage: %.1f%%", 
                            (float)high_task_count / total_runs * 100);
                    ESP_LOGI(TAG, "Medium priority percentage: %.1f%%", 
                            (float)med_task_count / total_runs * 100);
                    ESP_LOGI(TAG, "Low priority percentage: %.1f%%", 
                            (float)low_task_count / total_runs * 100);
                }
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== FreeRTOS Priority Scheduling Demo ===");
    
    // GPIO Configuration
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << LED_HIGH_PIN) | (1ULL << LED_MED_PIN) | (1ULL << LED_LOW_PIN),
        .pull_down_en = 0,
        .pull_up_en = 0,
    };
    gpio_config(&io_conf);

    // Button configuration
    gpio_config_t button_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = 1ULL << BUTTON_PIN,
        .pull_up_en = 1,
        .pull_down_en = 0,
    };
    gpio_config(&button_conf);

    ESP_LOGI(TAG, "Creating tasks with different priorities...");
    
    // Create tasks with different priorities
    xTaskCreate(high_priority_task, "HighPrio", 3072, NULL, 5, NULL);
    xTaskCreate(medium_priority_task, "MedPrio", 3072, NULL, 3, NULL);
    xTaskCreate(low_priority_task, "LowPrio", 3072, NULL, 1, NULL);
    xTaskCreate(control_task, "Control", 3072, NULL, 4, NULL);
    
    ESP_LOGI(TAG, "Press button to start priority test");
    ESP_LOGI(TAG, "Watch LEDs: GPIO2=High, GPIO4=Med, GPIO5=Low priority");
}
```

### Step 2: Round-Robin Scheduling (15 นาที)

เพิ่มการทดสอบ Round-Robin สำหรับ tasks ที่มี priority เท่ากัน:

```c
// เพิ่มใน main file

// Tasks ที่มี priority เท่ากัน
void equal_priority_task1(void *pvParameters)
{
    int task_id = 1;
    
    while (1) {
        if (priority_test_running) {
            ESP_LOGI(TAG, "Equal Priority Task %d running", task_id);
            
            // Simulate work without yielding
            for (int i = 0; i < 300000; i++) {
                volatile int dummy = i;
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(50)); // Short delay
    }
}

void equal_priority_task2(void *pvParameters)
{
    int task_id = 2;
    
    while (1) {
        if (priority_test_running) {
            ESP_LOGI(TAG, "Equal Priority Task %d running", task_id);
            
            // Simulate work without yielding
            for (int i = 0; i < 300000; i++) {
                volatile int dummy = i;
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(50)); // Short delay
    }
}

void equal_priority_task3(void *pvParameters)
{
    int task_id = 3;
    
    while (1) {
        if (priority_test_running) {
            ESP_LOGI(TAG, "Equal Priority Task %d running", task_id);
            
            // Simulate work without yielding
            for (int i = 0; i < 300000; i++) {
                volatile int dummy = i;
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(50)); // Short delay
    }
}

// เพิ่มใน app_main():
// สร้าง tasks ที่มี priority เท่ากัน (Priority 2)
xTaskCreate(equal_priority_task1, "Equal1", 2048, NULL, 2, NULL);
xTaskCreate(equal_priority_task2, "Equal2", 2048, NULL, 2, NULL);
xTaskCreate(equal_priority_task3, "Equal3", 2048, NULL, 2, NULL);
```

### Step 3: Priority Inversion Demo (10 นาที)

```c
// เพิ่มการสาธิต Priority Inversion (ปัญหาที่อาจเกิดขึ้น)

volatile bool shared_resource_busy = false;

void priority_inversion_high(void *pvParameters)
{
    while (1) {
        if (priority_test_running) {
            ESP_LOGW(TAG, "High priority task needs shared resource");
            
            // รอ shared resource
            while (shared_resource_busy) {
                ESP_LOGW(TAG, "High priority BLOCKED by low priority!");
                vTaskDelay(pdMS_TO_TICKS(10));
            }
            
            ESP_LOGI(TAG, "High priority task got resource");
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void priority_inversion_low(void *pvParameters)
{
    while (1) {
        if (priority_test_running) {
            ESP_LOGI(TAG, "Low priority task using shared resource");
            shared_resource_busy = true;
            
            // จำลองการใช้ resource นาน
            vTaskDelay(pdMS_TO_TICKS(2000));
            
            shared_resource_busy = false;
            ESP_LOGI(TAG, "Low priority task released resource");
        }
        
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}
```

## การทดสอบและวิเคราะห์

### การทดสอบ
1. **กดปุ่มเพื่อเริ่มการทดสอบ**
2. **สังเกต LED patterns**:
   - LED สว่างบ่อย = Task priority สูง
   - LED สว่างน้อย = Task priority ต่ำ
3. **ดู Serial Monitor**:
   - สังเกตลำดับการทำงาน
   - ดูสถิติการทำงาน

### การวิเคราะห์ผลลัพธ์
- **High Priority Task** ควรทำงานมากที่สุด
- **Medium Priority Task** ทำงานปานกลาง
- **Low Priority Task** ทำงานน้อยที่สุด
- **Equal Priority Tasks** ควรทำงานสลับกัน (Round-Robin)

## แบบฝึกหัด

### Exercise 1: เปลี่ยน Priority แบบ Dynamic

```c
void dynamic_priority_demo(void *pvParameters)
{
    TaskHandle_t low_task_handle = (TaskHandle_t)pvParameters;
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        
        ESP_LOGW(TAG, "Boosting low priority task to priority 4");
        vTaskPrioritySet(low_task_handle, 4);
        
        vTaskDelay(pdMS_TO_TICKS(2000));
        
        ESP_LOGW(TAG, "Restoring low priority task to priority 1");
        vTaskPrioritySet(low_task_handle, 1);
    }
}
```

### Exercise 2: Task Affinity (ESP32 Dual-Core)

```c
// Pin task ไปยัง specific core
xTaskCreatePinnedToCore(high_priority_task, "HighPrio", 3072, NULL, 5, NULL, 0); // Core 0
xTaskCreatePinnedToCore(low_priority_task, "LowPrio", 3072, NULL, 1, NULL, 1);   // Core 1
```

## คำถามสำหรับวิเคราะห์

1. Priority ไหนทำงานมากที่สุด? เพราะอะไร?
- High Priority Task ทำงานมากที่สุด เพราะ FreeRTOS ใช้ Priority-based Preemptive Scheduling ที่ให้ Task ที่มี Priority สูงสุดทำงานก่อนเสมอ
2. เกิด Priority Inversion หรือไม่? จะแก้ไขได้อย่างไร?
- เกิดได้เมื่อ Low Priority Task ครอบครองทรัพยากรที่ High Priority Task ต้องการ แก้ไขได้ด้วย Priority Inheritance
3. Tasks ที่มี priority เดียวกันทำงานอย่างไร?
- ใช้ Round-Robin Scheduling โดยสลับกันทำงานตาม Time Slice ที่กำหนด
4. การเปลี่ยน Priority แบบ dynamic ส่งผลอย่างไร?
- Task ที่ถูกเพิ่ม Priority จะได้รับ CPU มากขึ้นทันที และลด Priority กลับเมื่อไม่จำเป็น
5. CPU utilization ของแต่ละ priority เป็นอย่างไร?
- High Priority ใช้ CPU มากที่สุด รองลงมาคือ Medium และ Low ตามลำดับ

## ผลการทดลองที่คาดหวัง

| Priority | Expected Runs | Percentage | Behavior |
|----------|---------------|------------|----------|
| High (5) | มากที่สุด | 40-50% | ทำงานเร็วแล้วพัก |
| Medium (3) | ปานกลาง | 25-35% | ถูก preempt บ่อย |
| Low (1) | น้อยที่สุด | 15-25% | ทำงานเมื่อไม่มี priority สูงกว่า |

## บทสรุป

การทดลองนี้แสดงให้เห็นถึง:
1. **Priority-based Scheduling** - Task priority สูงจะทำงานก่อน
2. **Preemptive Nature** - RTOS จะขัดจังหวะ task ที่ priority ต่ำกว่า
3. **Round-Robin** - Tasks ที่ priority เท่ากันจะสลับกันทำงาน
4. **Priority Inversion** - ปัญหาที่อาจเกิดขึ้นและต้องระวัง
5. **Dynamic Priority** - สามารถเปลี่ยน priority ระหว่างทำงานได้

**Key Takeaways:**
- ออกแบบ priority อย่างรอบคอบ
- ระวัง priority inversion
- ใช้ appropriate delays
- Monitor task performance