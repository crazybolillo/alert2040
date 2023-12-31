#ifndef ALERT2040_ALERT2040_H
#define ALERT2040_ALERT2040_H

#define LED_ERROR 16
#define LED_WARNING 17
#define LED_PROXIMITY 15
#define LED_GROUP_MASK ((1 << LED_ERROR) | (1 << LED_WARNING) | (1 << LED_PROXIMITY))

#define ADC_PIN 28

#define TRIG_PIN 9
#define ECHO_PIN 8

struct HttpRequest {
    char *request_line;
    char *payload;
};

void init_http(void);
void http_task(void *params);

#endif  // ALERT2040_ALERT2040_H
