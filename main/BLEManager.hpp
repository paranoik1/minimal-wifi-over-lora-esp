#pragma once

#include <Arduino.h>
#include <BLE.h>
#include <gatt/BLECharacteristic.h>
#include "lora_transport.h"


class BLEManagerClass {
    private:
        BLEServer *server;
        BLEService *service;
        BLECharacteristic *ch_client_pkt;
        BLECharacteristic *ch_server_response_pkt;
        BLECharacteristic *ch_lora_configure;
        BLECharacteristic *ch_rssi_notification;

    public:
        BLEManagerClass() = default;
        // ~BLEManagerClass();

        void init();

        void onClientRequestPkt(BLECharacteristic::WriteHandler);
        void onClientConfigureLoRa(BLECharacteristic::WriteHandler);

        void notifyRssi(RSSI rssi);
        void notifyConfigureLoRa(const char *msg, size_t size);
        void notifyResponsePacket(const uint8_t* pkt_raw, size_t size);
        void startAdvertising();
};
