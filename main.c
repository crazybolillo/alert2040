#include <FreeRTOS.h>
#include <hardware/adc.h>
#include <pico/cyw43_arch.h>
#include <pico/stdlib.h>
#include <task.h>

#include "alert2040.h"

static const float ADC_RES = 3.3f / (1 << 12);

static volatile int led_off_time = 1980;
static volatile float distance = 0;
static volatile float voltage = 0;

static TaskHandle_t print_handle;
static TaskHandle_t adc_handle;

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    gpio_put(LED_ERROR, true);
    while (1) {}
}

/**
 * Constantly print out the readings and state of the system.
 * @param params
 */
void print_task(void *params) {
    while (1) {
        printf("ADC: %04.2f V\r\nDistance: %06.2f cm\r\033[A", voltage, distance);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/**
 * Read the analog voltage on ADC pin. This corresponds to an LDR in order to know whether the current
 * environment is dark or not.
 * @param params
 */
void adc_task(void *params) {
    while (1) {
        uint16_t reading = adc_read();
        voltage = (float)reading * ADC_RES;
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

/**
 * Start all the system tasks.
 *
 * During initialization blink LEDs five times so the user can perform a visual inspection on them.
 * Leave the error LED on and turn if after initialization is complete so the user knows the system failed to
 * start up if the LED stays on.
 */
void launch_task(void *params) {
    for (int count = 0; count < 5; count++) {
        gpio_put_masked(LED_GROUP_MASK, LED_GROUP_MASK);
        vTaskDelay(pdMS_TO_TICKS(200));
        gpio_put_masked(LED_GROUP_MASK, ~LED_GROUP_MASK);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    gpio_put(LED_ERROR, true);

    xTaskCreate(print_task, "print", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 2, &print_handle);
    configASSERT(&print_handle);

    xTaskCreate(adc_task, "adc", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 3, &adc_handle);
    configASSERT(&adc_task);

    gpio_put(LED_ERROR, false);
    vTaskDelete(NULL);
}

void setup_hardware() {
    gpio_init_mask(LED_GROUP_MASK);
    gpio_set_dir_out_masked(LED_GROUP_MASK);

    adc_init();
    adc_gpio_init(ADC_PIN);
    adc_select_input(2);
}

int main() {
    stdio_init_all();
    setup_hardware();

    xTaskCreateAffinitySet(launch_task, "launch", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 6, 0x01, NULL);
    vTaskStartScheduler();

    return 0;
}
