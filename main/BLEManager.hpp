#pragma once

#include <Arduino.h>
#include <BLE.h>


// C-подобный синтаксис
// typedef struct ble_manager_data {
//     BLEServer *p_server;
//     BLEService *p_service;
//     BLECharacteristic *p_ch_LED;
//     BLECharacteristic *p_ch_notify;
// } BLEManagerData;

// Структура хранения BLE данных
struct BLEManagerData {
    BLEServer *server;
    BLEService *packets_service;
    BLECharacteristic *ch_client_packet;
    BLECharacteristic *ch_server_response;
};

BLEManagerData* configure_ble_server(void (*onWriteRequestPacketCHR)(BLECharacteristic c, const BLEConnInfo &conn));

void start_advertising_ble();


void set_ble_server_callbacks(BLEManagerData *data, void (*onConnect)(), void (*onDisconnect)(BLEManagerData *data, uint8_t reason));


void ble_notify_response_packet_raw(BLEManagerData *data, uint8_t* pkt_raw, size_t size_packet);
