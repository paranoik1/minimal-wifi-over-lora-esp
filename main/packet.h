#ifndef LORA_PACKET_H
#define LORA_PACKET_H

#include <stdint.h>


#define MAX_PACKET_SIZE         240
#define MAX_PAYLOAD_SIZE        (MAX_PACKET_SIZE - 10)
#define SIZE_METADATA_PACKET    5
#define BYTES_AFTER_PAYLOAD     1


// 0, 1, 2, 3, 4  и так далее по порядку
#define MSG_TYPE_LIST(X) \
X(MSG_REQUEST) \
X(RECEIVED_PACKETS_REPORT) \
X(RESPONSE_SERVER) \
X(ERROR_RESPONSE) \
X(ACK_SERVER) 


typedef enum _msg_type {
#define DECLARE_ENUM_VALUE(name) name,
    MSG_TYPE_LIST(DECLARE_ENUM_VALUE)
#undef DECLARE_ENUM_VALUE

} MessageType;


typedef struct _lora_packet {
    // Хранит USER_ID - 4 бита, 
    // MSG_TYPE - 4 бита (2^4 = 16 значений),
    uint8_t user_id;
    uint8_t msg_type; 
    // 4 бита
    uint8_t session_id;
    // 4 бита
    uint8_t flags;
    // Идентификатор пакета - нужен для корректной обработки пакетов клиентом в нужной последовательности
    uint16_t pkt_id;
    // Всегда перед payload, чтобы легче было контролировать индекс length_payload в пакете
    uint8_t length_payload;
    // Указатель на массив байтов в куче - необходимо очищать перед очисткой самого пакета
    uint8_t* encrypted_payload;
    // Контрольная сумма всего пакета (кроме самого поля crc8)
    uint8_t crc8;
} Packet;

#ifdef __cplusplus
extern "C" {
#endif

void print_packet(Packet* pkt);

// `size_t* size_pkt_buff` - переменная, в которое будет записано значение размера всего пакета. 
// Возвращает указатель на массив байтов в куче - необходимо очистить после использования (`free(pkt_raw)`)
uint8_t* packet_to_bytes(Packet* pkt, size_t* size_pkt_buff);

// Создает в куче пакет (`malloc`) - необходимо очищать после обработки
Packet* packet_from_bytes(const uint8_t* pkt_raw);

uint8_t packet_raw_calculate_crc8(const uint8_t* pkt, size_t size_pkt);

void free_packet(Packet **pkt);

#ifdef __cplusplus
}
#endif

#endif
