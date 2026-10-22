// main/main.cpp
#include "BLEManager.hpp"
#include "driver/gpio.h"
#include "freertos/projdefs.h"
#include "hal/gpio_types.h"

#if CONFIG_USE_MOCK_TRANSPORT
#   include "transport_uart.h"
#else
#   include "transport.h"
#endif


#define GPIO_LED    GPIO_NUM_2
#define BUFFER_SIZE 3000


void task_read_bytes(void *pvParameters) {
    uint8_t* buffer = (uint8_t*)std::malloc(BUFFER_SIZE);
    while (1) {
        size_t length = receive_packet(buffer, BUFFER_SIZE);
        if (length == 0) 
            vTaskDelay(pdMS_TO_TICKS(20));
        else
            printf("Read bytes: %s\n", (const char *)buffer);
    }
}


extern "C" void app_main(void)
{
    gpio_reset_pin(GPIO_LED);
    gpio_set_direction(GPIO_LED, GPIO_MODE_OUTPUT);

    // Инициализация Arduino runtime
    initArduino();
    Serial.begin(115200);
    init_transport();
    
    BLEManagerData *data = configure_ble_server([](BLECharacteristic chr, const BLEConnInfo &conn) {
        Serial.println("Sending packet...");
        size_t size_packet = 0;
        const uint8_t* packet = chr.getValue(&size_packet);
        Serial.println(chr.getStringValue());
        send_packet(packet, size_packet);
        Serial.println("Packet succefull send!");
    });
    if (data == nullptr) {
        Serial.println("BLE configuration failed!");
        return;
    }

    set_ble_server_callbacks(
        data, 
        []() {
            Serial.println("Connected");
            gpio_set_level(GPIO_LED, 1);
            vTaskDelay(pdMS_TO_TICKS(500)); 
            gpio_set_level(GPIO_LED, 0);
        },
        [](BLEManagerData *cur_data, uint8_t reason) {
            Serial.printf("Disconnected, reason: 0x%02X\n", reason);
            // start_advertising_ble();
            for (int i = 0; i < 2; i++) {
                gpio_set_level(GPIO_LED, 1);
                vTaskDelay(pdMS_TO_TICKS(500)); 
                gpio_set_level(GPIO_LED, 0);
                vTaskDelay(pdMS_TO_TICKS(500)); 
            }
        } 
    );

    start_advertising_ble();
    Serial.println("BLE Server started successfully!");

    xTaskCreate(
        task_read_bytes,
        "Read Bytes",
        2048,
        NULL,
        2,
        NULL
    );
    // // В реальном проекте лучше вынести в отдельную задачу FreeRTOS
    // while (1) {
    //     vTaskDelay(pdMS_TO_TICKS(10));  // Не блокируем CPU
    // }
}
