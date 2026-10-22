#ifndef LORA_PROTOCOL_H
#define LORA_PROTOCOL_H

#include <stdint.h>

typedef enum _msg_type {
    // Запрос клиента к серверу (что делать с payload сервер определяет по его содержимому:)
    // если получен в payload url - скачать сайт
    // если команда, которая есть в MAP_COMMANDS (словарь за ранее прописанных комманд) - вызов комманды
    // иначе -> failed
    MSG_REQUEST,
    SERVER_RESPONSE_PACKET,
    // В полезной нагрузке содержится информация о полученных пакетах от сервера
    MSG_PACKETS_REPORT,
    FINAL_RESPONSE_PACKET,
    ERROR_SERVER_RESPONSE
} MessageType;

typedef struct lora_protocol {
    // Хранит USER_ID - 3 бита, 
    // MSG_TYPE - 3 бита (2^3 = 8 значений),
    // RESERVE - 2 бита
    uint8_t user_id;
    uint8_t msg_type;
    // Идентификатор пакета - нужен для корректной обработки пакетов клиентом в нужной последовательности
    uint16_t pkg_id;
    uint8_t length_payload;
    uint8_t* encrypted_data;
    // Контрольная сумма всего пакета (кроме самого поля crc8)
    uint8_t crc8;
} LoraProtocol;


#endif
