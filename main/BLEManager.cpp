#include "BLEManager.hpp"
#include "gatt/BLECharacteristic.h"
#include "types/BLEProperty.h"
#include <cstdlib>
#include <iostream>


#define BLE_DEVICE_NAME                 "Mini_Wi-Fi_over_LoRa"


static const BLEUUID SERVICE_UUID("550e8400-e29b-41d4-a716-446655440001");
static const BLEUUID CHR_REQUEST_PACKET_UUID("550e8400-e29b-41d4-a716-446655440002");
static const BLEUUID CHR_RESPONSE_PACKET_UUID("550e8400-e29b-41d4-a716-446655440003");


BLEManagerData* configure_ble_server(void (*onWriteRequestPacketCHR)(BLECharacteristic c, const BLEConnInfo &conn)) {
    BLE.begin(BLE_DEVICE_NAME);

    BLEServer server = BLE.createServer();

    BLEService service = server.createService(SERVICE_UUID);
    server.advertiseOnDisconnect(true);
    
    BLECharacteristic chr = service.createCharacteristic(
        CHR_REQUEST_PACKET_UUID,
        BLEProperty::Write,
        BLEPermission::WriteEncrypted
    );
    chr.onWrite([onWriteRequestPacketCHR](BLECharacteristic c, const BLEConnInfo &conn) {
        onWriteRequestPacketCHR(c, conn);
    });
    
    BLECharacteristic chr_notify = service.createCharacteristic(
        CHR_RESPONSE_PACKET_UUID,
        BLEProperty::Notify | BLEProperty::Read,
        BLEPermission::ReadEncrypted
    );

    BTStatus status = server.start();
    if (status != BTStatus::OK) {
        Serial.println("Failed start server");
        return nullptr;
    }

    BLEManagerData* data = (BLEManagerData*)std::malloc(sizeof(BLEManagerData));
    if (data == nullptr) {
        Serial.println("Failed to allocate BLEManagerData");
        return nullptr;
    }
    data->ch_client_packet = &chr;
    data->ch_server_response = &chr_notify;
    data->p_server = &server;
    data->p_service = &service;

    return data; 
}


void start_advertising_ble() {
    BLEAdvertising advertising = BLE.getAdvertising();
    BTStatus status = advertising.start();

    // BLEAdvertising pAdvertising = data->p_server->getAdvertising();
    // pAdvertising.addServiceUUID(SERVICE_UUID);
    // pAdvertising.setScanResponse(true);
    // BTStatus status = pAdvertising.start();

    if (status != BTStatus::OK) {
        Serial.println("Failed start advertising...");
        Serial.println(strcat((char*)"Status returned: ", status.toString()));
        std::cout << "Advertising is started: " << advertising.isAdvertising() << std::endl;
    }
}

void set_ble_server_callbacks(
    BLEManagerData *data, 
    void (*onConnect)(), 
    void (*onDisconnect)(BLEManagerData *data, uint8_t reason)
) {
    BLEServer server = *data->p_server;
    server.onConnect([onConnect](BLEServer s, const BLEConnInfo &conn) {
        onConnect();
    });
    server.onDisconnect([onDisconnect, data](BLEServer s, const BLEConnInfo &conn, uint8_t reason) {
        onDisconnect(data, reason);
    });
}


void ble_notify_response_packet_raw(BLEManagerData *data, uint8_t* pkt_raw, size_t size_packet) {
    data->ch_server_response->setValue(pkt_raw, size_packet);
    BTStatus status = data->ch_server_response->notify(pkt_raw, size_packet);

    if (status != BTStatus::OK) {
        std::cout << "Ошибка при notify пакета" << std::endl;
    }
}
