#include "BLEManager.hpp"
#include "esp_log.h"
#include "gatt/BLECharacteristic.h"
#include "types/BLEProperty.h"


#define BLE_DEVICE_NAME        "Mini_Wi-Fi_over_LoRa"
#define LOG_TAG                "BLEManager"         


static const BLEUUID SERVICE_UUID("550e8400-e29b-41d4-a716-446655440001");
static const BLEUUID CHR_REQUEST_PACKET_UUID("550e8400-e29b-41d4-a716-446655440002");
static const BLEUUID CHR_RESPONSE_PACKET_UUID("550e8400-e29b-41d4-a716-446655440003");


void onSubcribeLog(const BLECharacteristic &chr, const BLEConnInfo &conn, uint16_t subValue) {
    ESP_LOGI(LOG_TAG, "Новый подписчик на уведомления");
    ESP_LOGI(LOG_TAG, "Подписок: %d", chr.getSubscribedCount());
}


BLEManagerData* configure_ble_server(void (*onWriteRequestPacketCHR)(BLECharacteristic c, const BLEConnInfo &conn)) {
    BLE.begin(BLE_DEVICE_NAME);

    BLEServer* server = new BLEServer(BLE.createServer());

    BLEService* service = new BLEService(server->createService(SERVICE_UUID));
    server->advertiseOnDisconnect(true);
    
    BLECharacteristic* chr_client_packet = new BLECharacteristic(
        service->createCharacteristic(
            CHR_REQUEST_PACKET_UUID,
            BLEProperty::Write,
            BLEPermission::WriteEncrypted
        )
    );
    
    chr_client_packet->onWrite([onWriteRequestPacketCHR](BLECharacteristic c, const BLEConnInfo &conn) {
        onWriteRequestPacketCHR(c, conn);
    });
    
    BLECharacteristic* chr_notify_server_packets = new BLECharacteristic( 
        service->createCharacteristic(
            CHR_RESPONSE_PACKET_UUID,
            BLEProperty::Notify | BLEProperty::Read,
            BLEPermission::ReadOpen
        )
    );
    // chr_notify_server_packets->onStatus(onStatusIndicate);
    chr_notify_server_packets->onSubscribe(onSubcribeLog);

    BTStatus status = server->start();
    if (status != BTStatus::OK) {
        ESP_LOGE(LOG_TAG, "Failed start server");
        return nullptr;
    }

    BLEManagerData* data = new BLEManagerData(); //(BLEManagerData*)std::malloc(sizeof(BLEManagerData));
    if (data == nullptr) {
        ESP_LOGE(LOG_TAG, "Failed to allocate BLEManagerData");
        return nullptr;
    }
    data->ch_client_packet = chr_client_packet;
    data->ch_server_response = chr_notify_server_packets;
    data->server = server;
    data->packets_service = service;

    return data; 
}


void start_advertising_ble() {
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

void set_ble_server_callbacks(
    BLEManagerData *data, 
    void (*onConnect)(), 
    void (*onDisconnect)(BLEManagerData *data, uint8_t reason)
) {
    BLEServer server = *data->server;
    server.onConnect([onConnect](BLEServer s, const BLEConnInfo &conn) {
        onConnect();
    });
    server.onDisconnect([onDisconnect, data](BLEServer s, const BLEConnInfo &conn, uint8_t reason) {
        onDisconnect(data, reason);
    });
}


void ble_notify_response_packet_raw(BLEManagerData *data, uint8_t* pkt_raw, size_t size_packet) {
    data->ch_server_response->setValue(pkt_raw, size_packet);
    
    // Логируем размер и количество подписчиков ПЕРЕД отправкой
    ESP_LOGI(LOG_TAG, "Отправка notify: размер=%zuB, подписчиков=%d", 
             size_packet, data->ch_server_response->getSubscribedCount());
    
    BTStatus status = data->ch_server_response->notify(); 
    
    // Логируем статус
    if (status == BTStatus::OK) {
        // indication_pending = true;
        ESP_LOGI(LOG_TAG, "Notify инициирован");
    } else {
        ESP_LOGE(LOG_TAG, "Ошибка при вызове notify(): %s", status.toString());
        // Флаг НЕ ставим, так как indication не ушла
    }
}
