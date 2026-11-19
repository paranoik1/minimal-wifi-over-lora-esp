// main/main.cpp
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <driver/gpio.h>
#include <esp_log_level.h>
#include <freertos/idf_additions.h>
#include <freertos/projdefs.h>
#include <hal/gpio_types.h>

#include "esp_err.h"
#include "esp_timer.h"
#include "packet.h"
#include "BLEManager.hpp"
#include "lora_transports/lora_transport.h"
#include "settings_lora.hpp"


#define MAX_ITEMS_QUEUE     20
#define LOG_TAG             "main"


QueueHandle_t client_received_packets_queue;
QueueHandle_t server_received_packets_queue;
QueueHandle_t send_packets_queue;


// Я не особо разобрался, что такое inline, поэтому просто удалил его навсякий
BLEManagerClass BLEManager;
static volatile RSSI last_rssi = 0;


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
        RSSI rssi = 0;
        Packet *pkt = transport_receive_packet(&rssi);

        if (pkt == NULL) continue;

        last_rssi = rssi;

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
    while (1) {
        Packet *pkt = NULL;
        if (xQueueReceive(server_received_packets_queue, &pkt, pdMS_TO_TICKS(20)) != pdTRUE) 
            continue;

        size_t pkt_size = 0;
        uint8_t* pkt_raw = packet_to_bytes(pkt, &pkt_size);
        // длина пакета без crc8
        uint8_t calculated_crc8 = packet_raw_calculate_crc(pkt_raw, pkt_size - 1);

        if (pkt->crc8 != calculated_crc8) {
            ESP_LOGD("process_pkt", "Несоответствие контрольных сумм: pkt.crc8 (%d) != calculated_crc8 (%d) )", pkt->crc8, calculated_crc8);
            goto free_pkt; 
        }

        ESP_LOGD("process_pkt", "валидный пакет: ");
        print_packet(pkt);
        BLEManager.notifyResponsePacket(pkt_raw, pkt_size);

    free_pkt:
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
    // esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("BLEManager", ESP_LOG_VERBOSE);
    esp_log_level_set("transport", ESP_LOG_DEBUG);
}


static void timer_notify_last_rssi(void *params) {
    ESP_LOGI("timer_notify_rssi", "Notify RSSI: %d", last_rssi);
    BLEManager.notifyRssi(last_rssi);
}


esp_timer_create_args_t timer_args = {
    .callback = &timer_notify_last_rssi,
    .arg = NULL,
    .name = "RSSI Timer",
}; // type: ignore
esp_timer_handle_t timer_handler = NULL;
bool timer_started_success = false;


void on_client_request_packet(BLECharacteristic chr, const BLEConnInfo &conn) {
    const char *tag = "request_pkt";

    ESP_LOGI(tag, "Получен сформированный пакет от клиента...");
    size_t size_packet = 0;
    const uint8_t* pkt_raw = chr.getValue(&size_packet);
    Packet* packet = packet_from_bytes(pkt_raw);

    queue_send_and_wait(client_received_packets_queue, packet);

    if (timer_started_success) {
        ESP_LOGI(tag, "timer уже существует и запущен");
        esp_timer_dump(stdout);
        return;
    }

    esp_err_t err = esp_timer_create(&timer_args, &timer_handler);
    if (err != ESP_OK) {
        ESP_LOGE(tag, 
            ">> ошибка при создании timer = %s", esp_err_to_name(err)
        );
        while(1);
    }

    err = esp_timer_start_periodic(timer_handler, 5 * 1000 * 1000);
    if (err != ESP_OK) {
        ESP_LOGE(tag, 
            ">> ошибка при запуске = %s", esp_err_to_name(err)
        );
        while(1);
    }

    timer_started_success = true;
}


void on_client_configure_lora(BLECharacteristic chr, const BLEConnInfo &conn) {
    String settings = chr.getStringValue();

    if (settings == "HELP") {
        
        return;
    }

    char notify_message[300] = {0};
    esp_err_t err = apply_settings_list(settings, notify_message, sizeof(notify_message));
    if (err != ESP_OK) {
        ESP_LOGE("on_client_configure_lora", 
            "Ошибка - превышена длина nontify_message: %d, %s", 
            sizeof(notify_message), esp_err_to_name(err));
    }

    BLEManager.notifyConfigureLoRa(notify_message, strlen(notify_message));
}



extern "C" void app_main(void)
{
    // Инициализация очередей
    init_queue_tasks();
    // Инициализация Arduino runtime
    initArduino();
    // Инициализациия транспорта (передача и получение пакетов)
    init_transport();
    // Нужно вызывать после initArduino, в ином случае настройки, сделанные функцией ниже перезапишутся функцией initArduino
    configure_logging();
    // Для работы BLE.h сначало необходимо инициализировать initArduino 
    BLEManager.init();

    esp_err_t error = lora_configure();

    ESP_LOGI(LOG_TAG, "lora_configure: %s",
            esp_err_to_name(error));

    BLEManager.onClientRequestPkt(on_client_request_packet);
    BLEManager.onClientConfigureLoRa(on_client_configure_lora);
    BLEManager.startAdvertising();
    printf("BLE Server started successfully!\n");

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
        NULL,
        1,
        NULL
    );

    int last_edges = get_transport_edges();

    while (1) {
        int cur_edges = get_transport_edges();

        if (last_edges != cur_edges) {
            ESP_LOGI("ISR", "edges = %d -> %d", last_edges, cur_edges);
            last_edges = cur_edges;
        }

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
