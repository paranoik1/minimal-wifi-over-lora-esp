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

struct BLEManagerData {
    BLEServer *p_server;
    BLEService *p_service;
    BLECharacteristic *p_ch_LED;
    BLECharacteristic *p_ch_notify;
};

BLEManagerData* configure_ble_server(void (*onWriteRequestPacketCHR)(BLECharacteristic c, const BLEConnInfo &conn));

void start_advertising_ble();


void set_ble_server_callbacks(BLEManagerData *data, void (*onConnect)(), void (*onDisconnect)(BLEManagerData *data, uint8_t reason));
