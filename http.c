#include <FreeRTOS.h>
#include <lwip/tcp.h>
#include <pico/cyw43_arch.h>
#include <sys/cdefs.h>
#include <task.h>

#include "alert2040.h"

static char default_headers[256];
static uint32_t default_headers_size;

static char content_length_header[64];
static ip_addr_t api_addr;

extern volatile int blink_off_ms;
extern volatile bool wifi_connected;

static void httpSend(struct tcp_pcb *tpcb, struct HttpRequest *request) {
    size_t payload_size;

    tcp_write(tpcb, request->request_line, strlen(request->request_line), TCP_WRITE_FLAG_MORE);
    if (request->payload != NULL) {
        payload_size = strlen(request->payload);
        snprintf(content_length_header, 64, "Content-Length: %d\r\n", payload_size);
        tcp_write(tpcb, content_length_header, strlen(content_length_header), TCP_WRITE_FLAG_MORE);
        tcp_write(tpcb, default_headers, default_headers_size, TCP_WRITE_FLAG_MORE);
        tcp_write(tpcb, request->payload, payload_size, 0);
    } else {
        tcp_write(tpcb, default_headers, default_headers_size, 0);
    }
    tcp_output(tpcb);
}

static void httpErrHandler(void *arg, err_t err) {
    gpio_put(LED_ERROR, true);
    busy_wait_ms(200);
    gpio_put(LED_ERROR, false);
}

static err_t httpRecvHandler(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    pbuf_free(p);
    return tcp_close(tpcb);
}

static err_t httpConnectHandler(void *arg, struct tcp_pcb *tpcb, err_t err) {
    if (arg != NULL) {
        struct HttpRequest *request = (struct HttpRequest *)arg;
        httpSend(tpcb, request);
    } else {
        tcp_close(tpcb);
    }

    return 0;
}

/**
 * Perform a HTTP 1.1 request.
 * This task waits permanently to be notified for running. A 32 bit pointer to a HttpRequest
 * struct must be passed through the notification. Said HttpRequest specifies the URL, METHOD
 * and payload to use.
 * @param params
 */
void http_task(__unused void *pvParameters) {
    IP4_ADDR(&api_addr, API_IPV4_B1, API_IPV4_B2, API_IPV4_B3, API_IPV4_B4);
    snprintf(
        default_headers, 256,
        "Host: %s:%d\r\n"
        "Accept: application/json\r\n"
        "Content-Type: application/json\r\n"
        "Connection: close\r\n\r\n",
        API_IPV4, API_PORT
    );
    default_headers_size = strlen(default_headers);
    while (1) {
        struct HttpRequest *request = (struct HttpRequest *)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (wifi_connected == false) { continue; }

        struct tcp_pcb *connection = tcp_new();
        tcp_arg(connection, request);
        tcp_err(connection, httpErrHandler);
        tcp_recv(connection, httpRecvHandler);

        if (tcp_connect(connection, &api_addr, API_PORT, httpConnectHandler) != ERR_OK) {
            gpio_put(LED_ERROR, true);
            vTaskDelay(pdMS_TO_TICKS(500));
            gpio_put(LED_ERROR, false);
        }
    }
}
