#include <stdio.h>
#include "mqtt_broker_service.h"
#include "mosq_broker.h"
#include "esp_log.h"

static const char *TAG = "mqtt_broker";

void mqtt_broker_start(void)
{
    struct mosq_broker_config config = {
        .host = "0.0.0.0",
        .port = 1883,
        .tls_cfg = NULL
    };

    ESP_LOGI(TAG, "Starting MQTT broker");

    mosq_broker_run(&config);
}

















