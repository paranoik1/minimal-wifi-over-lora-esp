#include "packet.h"
#include "esp_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define LOG_TAG     "packet"


// #name - превращает макрос параметр в строку (name = MSG_REQUEST => #name = "MSG_REQUEST")
const char* __get_msg_type_name(uint8_t msg_type) {
    switch (msg_type) {
#define DECLARE_CASE(name) \
        case name: return #name;
        MSG_TYPE_LIST(DECLARE_CASE)
#undef DECLARE_CASE
        default:
            return "UNKNOWN";
    }
}


void __convert_flags_to_string(char* buff, uint8_t flags) {
	int counter = 0; 
    for (int i = FLAGS_BITS - 1; i != -1; i--) {
        if (flags & (1 << i))
            buff[counter] = '1';
        else
            buff[counter] = '0';

		counter++;
    }
}




void packet_to_bytes(Packet *pkt, uint8_t* buff, size_t* size_pkt_buff) {
    uint8_t* pkt_bytes = buff;

    pkt_bytes[0] = (pkt->user_id << 4) | pkt->msg_type;
    pkt_bytes[1] = (pkt->session_id << 4) | pkt->flags;
    pkt_bytes[2] = (uint8_t)(pkt->pkt_id >> 8);
    pkt_bytes[3] = (uint8_t)pkt->pkt_id;

    uint8_t length_payload = pkt->length_payload;
    pkt_bytes[4] = length_payload;
    
    for (int i = 0; i < length_payload; i++) {
        pkt_bytes[SIZE_METADATA_PACKET + i] = pkt->encrypted_payload[i];
    }

    // packet_calculate_crc8(pkt_bytes, length_payload + 5)
    pkt_bytes[SIZE_METADATA_PACKET + length_payload] = pkt->crc8;

    *size_pkt_buff = SIZE_METADATA_PACKET + length_payload + BYTES_AFTER_PAYLOAD;
}


Packet* packet_from_bytes(const uint8_t* pkt_raw) {
    if (pkt_raw == NULL) {
        // %p - тип для вывода указателей
        ESP_LOGE(LOG_TAG, 
            ">> packet_from_bytes - *pkt_raw = NULL");
        return NULL;
    }

    uint8_t header = pkt_raw[0];
    uint8_t session_info = pkt_raw[1];
    uint16_t pkt_id = (uint16_t)pkt_raw[2] << 8 | (uint16_t)pkt_raw[3];
    uint8_t length_payload = pkt_raw[4];

    uint8_t* payload = malloc(length_payload);
    memset(payload, 0, length_payload);

    for (int i = 0; i < length_payload; i++) {
        payload[i] = pkt_raw[SIZE_METADATA_PACKET + i];
    } 

    uint8_t crc8 = pkt_raw[SIZE_METADATA_PACKET + length_payload];

    Packet* pkt_ptr = malloc(sizeof(Packet));
    if (pkt_ptr == NULL) {
        // payload был выделен выше
        free(payload);  
        return NULL;
    }

    pkt_ptr->user_id = header >> 4;
    pkt_ptr->msg_type = header & 0x0F;
    pkt_ptr->session_id = session_info >> 4;
    pkt_ptr->flags = session_info & 0x0F;
    pkt_ptr->pkt_id = pkt_id;
    pkt_ptr->length_payload = length_payload;
    pkt_ptr->encrypted_payload = payload;
    pkt_ptr->crc8 = crc8;

    return pkt_ptr;
}


void print_packet(Packet *pkt) {
    char flags_string[FLAGS_BITS + 1] = {0};
    __convert_flags_to_string(flags_string, pkt->flags);
    // NOTE: может быть непредвиденное поведение при выводе такой строки (без конечного символа \0)
    flags_string[FLAGS_BITS] = '\0';

    const char* msg_type = __get_msg_type_name(pkt->msg_type);

    printf(
        "user_id=%d msg_type=%s session_id=%d flags=%s pkg_id=%d length_payload=%d crc8=%d\n",
        pkt->user_id, msg_type, pkt->session_id, flags_string, pkt->pkt_id, pkt->length_payload, pkt->crc8
    );
}


uint8_t packet_raw_calculate_crc(const uint8_t *pkt, size_t size_pkt_without_crc) {
    uint8_t crc = 0x00;
    uint8_t poly = 0x07;

    for (int i = 0; i < size_pkt_without_crc; i++) {
        uint8_t byte = pkt[i];
        crc ^= byte;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80)
                crc = (crc << 1) ^ poly;
            else
                crc <<= 1;
            crc &= 0xFF;
        }
    }

    return crc;
}


void free_packet(Packet **pkt) {
    Packet *packet = *pkt;
    free(packet->encrypted_payload);
    free(packet);

    *pkt = NULL;
}
