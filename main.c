#include <FreeRTOS.h>
#include <hardware/adc.h>
#include <pico/cyw43_arch.h>
#include <pico/multicore.h>
#include <pico/stdlib.h>
#include <task.h>

#include "alert2040.h"

enum { BUFFER_SIZE = 64 };

static const float DISTANCE_THRESHOLD = 40.0F;
static const float ADC_LDR_THRESHOLD = 2.2F;

static const float ADC_RES = 3.3f / (1 << 12);

static volatile float distance = 999;
static volatile float voltage = 3.3;

static volatile uint32_t pulse_start = 0;
static volatile uint32_t pulse_end = 0;

static TaskHandle_t monitor_handle;
static TaskHandle_t usonic_handle;
static TaskHandle_t print_handle;
static TaskHandle_t http_handle;
static TaskHandle_t adc_handle;

static char buffer[BUFFER_SIZE];
static struct HttpRequest warningRequest = {.request_line = "POST / HTTP/1.1\r\n", .payload = buffer};

// ------------------------------
// ------ Shared variables ------
volatile int blink_off_ms = 1980;
volatile bool wifi_connected = false;
// ------------------------------

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    gpio_put(LED_ERROR, true);
    while (1) {}
}

/**
 * Blink the onboard LED to provide status. If it blinks rapidly it means the WiFi connection has failed.
 * @param params
 */
void blink_task(void *params) {
    while (1) {
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(20));
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(blink_off_ms));
    }
}

/**
 * Monitor the current light levels (ADC reading), the distance to the nearest object (ultrasonic sensor). Turn
 * on the corresponding LED lights if the readings fall below the given thresholds.
 * @param params
 */
void monitor_task(void *params) {
    while (1) {
        float distance_snap = distance;
        float voltage_snap = voltage;

        if (distance_snap <= DISTANCE_THRESHOLD) {
            gpio_put(LED_PROXIMITY, true);
            if (voltage_snap <= ADC_LDR_THRESHOLD) {
                gpio_put(LED_WARNING, true);
                snprintf(
                    buffer, BUFFER_SIZE, "{\"Info\": \"Voltage %04.2f --- Distance %06.2f\"}", voltage_snap,
                    distance_snap
                );
                xTaskNotify(http_handle, (uint32_t)&warningRequest, eSetValueWithOverwrite);
                vTaskDelay(pdMS_TO_TICKS(5000));
            } else {
                gpio_put(LED_WARNING, false);
            }
        } else {
            gpio_put(LED_WARNING, false);
            gpio_put(LED_PROXIMITY, false);
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
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
 * Measure the pulse-width in us generated by the HC-SR04 ultrasonic sensor.
 */
void ultrasonic_irq(void) {
    uint32_t events = gpio_get_irq_event_mask(ECHO_PIN);
    if (events & GPIO_IRQ_EDGE_RISE) { pulse_start = time_us_32(); }
    if ((events & GPIO_IRQ_EDGE_FALL) && (pulse_start != 0)) {
        pulse_end = time_us_32();
        vTaskNotifyGiveFromISR(usonic_handle, NULL);
    }

    gpio_acknowledge_irq(ECHO_PIN, events);
}

/**
 * Trigger an ultrasonic HC-SR04 sensor. Wait for the RX pulse to come in or timeout and trigger the sensor
 * again. If the RX pulse was detected, the corresponding distance is calculated based on the datasheet.
 * @param params
 */
void ultrasonic_task(void *params) {
    while (1) {
        pulse_start = pulse_end = 0;

        gpio_put(TRIG_PIN, true);
        sleep_us(10);
        gpio_put(TRIG_PIN, false);
        ulTaskNotifyTake(pdFALSE, pdMS_TO_TICKS(250));

        if (pulse_start != 0 && pulse_end != 0) { distance = (float)(pulse_end - pulse_start) / 58; }
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

    if (cyw43_arch_init_with_country(CYW43_COUNTRY_MEXICO) != 0) {
        while (1) {}
    }
    cyw43_arch_enable_sta_mode();

    printf("connecting to %s\r\n", WIFI_SSID);
    printf("using PSK %s and timeout %d ms\r\n\r\n", WIFI_PSK, WIFI_TIMEOUT);
    int err = cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PSK, CYW43_AUTH_WPA2_AES_PSK, WIFI_TIMEOUT);
    if (err) {
        printf("error %d\r\n", err);
        blink_off_ms = 300;
    } else {
        wifi_connected = true;
        blink_off_ms = 1980;
    }

    xTaskCreate(print_task, "print", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 2, &print_handle);
    configASSERT(&print_handle);

    xTaskCreate(adc_task, "adc", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 3, &adc_handle);
    configASSERT(&adc_task);

    xTaskCreate(ultrasonic_task, "usonic", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 3, &usonic_handle);
    configASSERT(&usonic_handle);

    xTaskCreate(monitor_task, "monitor", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 2, &monitor_handle);
    configASSERT(&monitor_handle);

    xTaskCreate(blink_task, "blink", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);

    xTaskCreate(http_task, "http", configMINIMAL_STACK_SIZE * 2, NULL, tskIDLE_PRIORITY + 2, &http_handle);
    configASSERT(&http_handle);

    irq_set_enabled(IO_IRQ_BANK0, true);
    gpio_put(LED_ERROR, false);

    vTaskDelete(NULL);
}

void setup_hardware() {
    gpio_init_mask(LED_GROUP_MASK);
    gpio_set_dir_out_masked(LED_GROUP_MASK);

    adc_init();
    adc_gpio_init(ADC_PIN);
    adc_select_input(2);

    gpio_init(TRIG_PIN);
    gpio_set_dir(TRIG_PIN, GPIO_OUT);

    gpio_init(ECHO_PIN);
    gpio_set_dir(ECHO_PIN, GPIO_IN);

    gpio_add_raw_irq_handler(ECHO_PIN, ultrasonic_irq);
    gpio_set_irq_enabled(ECHO_PIN, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true);
}

int main() {
    stdio_init_all();
    setup_hardware();

    xTaskCreateAffinitySet(launch_task, "launch", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, 0x01, NULL);
    vTaskStartScheduler();

    return 0;
}
