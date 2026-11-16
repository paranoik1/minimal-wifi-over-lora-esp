#include "BLEManager.hpp"

#include <cstdio>
#include <esp_log.h>
#include <types/BLEProperty.h>


#define BLE_DEVICE_NAME        "Mini_Wi-Fi_over_LoRa"
#define LOG_TAG                "BLEManager"         
#define GPIO_LED                GPIO_NUM_2


static const BLEUUID SERVICE_UUID("550e8400-e29b-41d4-a716-446655440001");
static const BLEUUID CHR_REQUEST_PACKET_UUID("550e8400-e29b-41d4-a716-446655440002");
static const BLEUUID CHR_RESPONSE_PACKET_UUID("550e8400-e29b-41d4-a716-446655440003");
static const BLEUUID CHR_LORA_CONFIGURE_UUID("550e8400-e29b-41d4-a716-446655440004");
static const BLEUUID CHR_NOTIFY_RSSI_UUID("550e8400-e29b-41d4-a716-446655440005");


static void onSubcribeLog(const BLECharacteristic &chr, const BLEConnInfo &conn, uint16_t subValue) {
    ESP_LOGI(LOG_TAG, "Новый подписчик на уведомления");
    ESP_LOGI(LOG_TAG, "Подписок: %d", chr.getSubscribedCount());
}


static void onConnect(const BLEServer &s, const BLEConnInfo &conn) {
    ESP_LOGI(LOG_TAG, "Connected");
    gpio_set_level(GPIO_LED, 1);
    vTaskDelay(pdMS_TO_TICKS(300)); 
    gpio_set_level(GPIO_LED, 0);
}


static void onDisconnect(const BLEServer &server, const BLEConnInfo &conn, uint8_t reason) {
    ESP_LOGI(LOG_TAG, "Disconnected, reason: 0x%02X", reason);
    for (int i = 0; i < 2; i++) {
        gpio_set_level(GPIO_LED, 1);
        vTaskDelay(pdMS_TO_TICKS(300)); 
        gpio_set_level(GPIO_LED, 0);
        vTaskDelay(pdMS_TO_TICKS(300)); 
    }
}


static void configureLED() {
    gpio_reset_pin(GPIO_LED);
    gpio_set_direction(GPIO_LED, GPIO_MODE_OUTPUT);
}

BLEManagerClass::~BLEManagerClass()  {
    delete server;
}


BLEManagerClass::BLEManagerClass() {
    configureLED();

    BLE.begin(BLE_DEVICE_NAME);

    server = new BLEServer(BLE.createServer());
    server->onConnect(onConnect);
    server->onDisconnect(onDisconnect);

    service = new BLEService(server->createService(SERVICE_UUID));
    server->advertiseOnDisconnect(true);
    
    ch_client_pkt = new BLECharacteristic(
        service->createCharacteristic(
            CHR_REQUEST_PACKET_UUID,
            BLEProperty::Write,
            BLEPermission::WriteOpen
        )
    );
    
    ch_server_response_pkt = new BLECharacteristic( 
        service->createCharacteristic(
            CHR_RESPONSE_PACKET_UUID,
            BLEProperty::Notify | BLEProperty::Read,
            BLEPermission::ReadOpen
        )
    );
    // ch_server_response_pkt->onStatus(onStatusIndicate);
    ch_server_response_pkt->onSubscribe(onSubcribeLog);

    ch_lora_configure = new BLECharacteristic(
        service->createCharacteristic(
            CHR_LORA_CONFIGURE_UUID, 
            BLEProperty::Write | BLEProperty::Read | BLEProperty::Notify,
            BLEPermission::WriteOpen | BLEPermission::ReadOpen
        )
    );

    ch_rssi_notification = new BLECharacteristic(
        service->createCharacteristic(
            CHR_NOTIFY_RSSI_UUID, 
            BLEProperty::Notify | BLEProperty::Read,
            BLEPermission::ReadOpen
        )
    );

    BTStatus status = server->start();
    if (status != BTStatus::OK) {
        ESP_LOGE(LOG_TAG, "Failed start server");
        while (1);
    }
}


void BLEManagerClass::startAdvertising() {
    BLEAdvertising advertising = BLE.getAdvertising();
    advertising.addServiceUUID(SERVICE_UUID);
    advertising.setScanResponse(true);

    BTStatus status = advertising.start();
    if (status != BTStatus::OK) {
        ESP_LOGE(
            LOG_TAG, 
            "Failed start advertising: status=%s, is_started=%d", 
            status.toString(), 
            advertising.isAdvertising()
        );
    }
}


static void ble_notify(BLECharacteristic *ch, const uint8_t *raw, size_t size) {
    ch->setValue(raw, size);
    
    // Логируем размер и количество подписчиков ПЕРЕД отправкой
    ESP_LOGD(LOG_TAG, "Отправка notify: размер=%zuB, подписчиков=%d", 
             size, ch->getSubscribedCount());
    BTStatus status = ch->notify();

    // Логируем статус
    if (status != BTStatus::OK) {
        ESP_LOGE(LOG_TAG, "Ошибка при вызове notify(): %s", status.toString());
    }
}

void BLEManagerClass::onClientRequestPkt(
    std::function<void(
        BLECharacteristic chr, const BLEConnInfo &conn
    )> handler
) {
    ch_client_pkt->onWrite(handler);
}


void BLEManagerClass::notifyResponsePacket(const uint8_t* pkt_raw, size_t size_packet) {
    ble_notify(ch_server_response_pkt, pkt_raw, size_packet);
}


void BLEManagerClass::notifyRssi(RSSI rssi) {
    char s[5] = {0};
    snprintf(s, sizeof s, "%d", rssi);
    ble_notify(ch_rssi_notification, (const uint8_t*)s, strlen(s));
}
