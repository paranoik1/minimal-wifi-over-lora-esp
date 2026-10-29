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

    BTStatus status = server->start();
    if (status != BTStatus::OK) {
        Serial.println("Failed start server");
        return nullptr;
    }

    BLEManagerData* data = new BLEManagerData(); //(BLEManagerData*)std::malloc(sizeof(BLEManagerData));
    if (data == nullptr) {
        Serial.println("Failed to allocate BLEManagerData");
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
    std::cout << "Subscribers: " << data->ch_server_response->getSubscribedCount() << std::endl;
    BTStatus status = data->ch_server_response->notify();//notify(pkt_raw, size_packet);

    if (status != BTStatus::OK) {
        std::cout << "Ошибка при notify пакета: " << status.toString() << std::endl;
    }
}
