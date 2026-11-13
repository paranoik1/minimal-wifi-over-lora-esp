// main/main.cpp
#include "BLEManager.hpp"
#include "driver/gpio.h"
#include "esp_log_level.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "hal/gpio_types.h"
#include "packet.h"


#if CONFIG_USE_MOCK_TRANSPORT
#   include "transport_uart.h"
#else
#   include "transport.h"
#endif


#define GPIO_LED            GPIO_NUM_2
#define MAX_ITEMS_QUEUE     20
#define LOG_TAG             "main"

QueueHandle_t client_received_packets_queue;
QueueHandle_t server_received_packets_queue;
QueueHandle_t send_packets_queue;


void queue_send_and_wait(QueueHandle_t queue, Packet *pkt) {
    while (1) {
        if (xQueueSend(queue, &pkt, pdMS_TO_TICKS(20)) != pdTRUE) {
            ESP_LOGD(LOG_TAG, "ОЧередь заполнена, ждем");
            continue;
        } 
        break;
    }
}


/*
Таска для приема сообщений от LoRa и закидваний их в очередь server_received_packets_queue (не занимается проверкой контрольной суммы и ID пользователя)
*/
void task_receive_server_packets(void *pvParameters) {
    while (1) {
        Packet *pkt = transport_receive_packet();
        if (pkt == NULL) continue;

        queue_send_and_wait(server_received_packets_queue, pkt);

        printf("Receive packet: ");
        print_packet(pkt);
    }
}


/*
Таска для отправки сообщений по LoRa (берет пакеты из очереди send_packets_queue)
*/
void task_send_packets_to_server(void *pvParameters) {
    while (1) {
        Packet* pkt = NULL;
        if (xQueueReceive(send_packets_queue, &pkt, pdMS_TO_TICKS(20)) != pdTRUE)
            continue;

        transport_send_packet(pkt);
        free_packet(&pkt);
    }
}


/*
Таска для обработки полученных пакетов (сообщений) от сервера (проверка контрольной суммы, проверка user_id) с последующей отправкой по BLE клиенту
*/
void task_process_server_packets(void *pvParameters) {
    BLEManagerData* ble_data = (BLEManagerData*)pvParameters; 
    while (1) {
        Packet *pkt = NULL;
        if (xQueueReceive(server_received_packets_queue, &pkt, pdMS_TO_TICKS(20)) != pdTRUE) 
            continue;

        size_t pkt_size = 0;
        uint8_t* pkt_raw = packet_to_bytes(pkt, &pkt_size);
        // длина пакета без crc8
        uint8_t calculated_crc8 = packet_raw_calculate_crc8(pkt_raw, pkt_size - 1);

        if (pkt->crc8 != calculated_crc8) {
            printf("Получен невалидный пакет (несоответсвие контрольных сумм: pkt.crc8 %d != %d - calculated_crc8)\n", pkt->crc8, calculated_crc8);
            goto free_pkt; 
        }

        ble_notify_response_packet_raw(ble_data, pkt_raw, pkt_size);

    free_pkt:
        printf("Очистка кучи: ");
        print_packet(pkt);
        free(pkt_raw);
        free_packet(&pkt);

        vTaskDelay(pdMS_TO_TICKS(12));
    }
}





void __init_queue(QueueHandle_t *queue, const char* error_message) {
    *queue = xQueueCreate(MAX_ITEMS_QUEUE, sizeof(Packet*));
    if (queue == NULL) {
        printf("%s\n", error_message);
        while(1);
    }
}

/*
Инициализация очередей для работы с пакетами в параллельном режиме (ну почти)
*/
void init_queue_tasks() {
    __init_queue(&server_received_packets_queue, "Queue server_received_packets is NULL");
    __init_queue(&client_received_packets_queue, "Queue client_received_packets is NULL");
    __init_queue(&send_packets_queue, "Queue send_packets is NULL");
}


void configure_logging() {
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("BLEManager", ESP_LOG_VERBOSE);
    esp_log_level_set("transport", ESP_LOG_DEBUG);
}


extern "C" void app_main(void)
{
    configure_logging();

    gpio_reset_pin(GPIO_LED);
    gpio_set_direction(GPIO_LED, GPIO_MODE_OUTPUT);

    // Инициализация очередей
    init_queue_tasks();
    // Инициализация Arduino runtime
    initArduino();
    // Инициализациия транспорта (передача и получение пакетов)
    init_transport();

    Serial.begin(115200);

    
    BLEManagerData *ble_data = configure_ble_server([](BLECharacteristic chr, const BLEConnInfo &conn) {
        Serial.println("Получен сформированный пакет...");
        size_t size_packet = 0;
        const uint8_t* pkt_raw = chr.getValue(&size_packet);
        Packet* packet = packet_from_bytes(pkt_raw);
        queue_send_and_wait(client_received_packets_queue, packet);
    });
    if (ble_data == nullptr) {
        Serial.println("BLE configuration failed!");
        while(1);
    }

    set_ble_server_callbacks(
        ble_data, 
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
        task_receive_server_packets,
        "Receive server packets",
        2048,
        NULL,
        2,
        NULL
    );

    xTaskCreate(
        task_send_packets_to_server,
        "Send packets",
        2048,
        NULL,
        2,
        NULL
    );

    xTaskCreate(
        task_process_server_packets,
        "Process server packets",
        2048,
        ble_data,
        1,
        NULL
    );

    while (1) {
        Packet *pkt = NULL;
        if (xQueueReceive(client_received_packets_queue, &pkt, pdMS_TO_TICKS(20)) != pdTRUE) {
            continue;
        }

        if (pkt == NULL) {
            printf("Empty packet\n");
            continue;
        }
        
        printf("Отправляем пакет: ");
        print_packet(pkt);
        queue_send_and_wait(send_packets_queue, pkt);
    }
}
