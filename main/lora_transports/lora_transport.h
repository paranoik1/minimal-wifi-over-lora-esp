#ifndef TRANSPORT_H
#define TRANSPORT_H


#include "esp_err.h"
#include "packet.h"


typedef int8_t RSSI;

typedef enum {
    LORA_CR_4_5 = 5,
    LORA_CR_4_6 = 6,
    LORA_CR_4_7 = 7,
    LORA_CR_4_8 = 8
} lora_coding_rate_t;


#ifdef __cplusplus
extern "C" {
#endif

int get_transport_state();

int get_transport_edges();

void init_transport();


Packet* transport_receive_packet(RSSI* rssi);

void transport_send_packet(Packet *packet);

/*
 * Изменить мощность передачи модуля (обычно от 1 до 22 dBm,
 * допустимый диапазон зависит от модуля).
*/
esp_err_t lora_set_power(uint8_t power);

/*
 * Изменить Spreading Factor (SF) — коэффициент расширения спектра,
 * определяющий количество чипов на один символ LoRa.
 */
esp_err_t lora_set_sf(uint8_t sf);

/*
 * Изменить ширину полосы пропускания канала (Bandwidth).
 * Чем выше значение, тем выше потенциальная скорость передачи,
 * но ниже чувствительность приёма.
 */
esp_err_t lora_set_bw(float bw);

/*
 * Изменить скорость кодирования (Coding Rate).
 * Определяет соотношение полезных данных и избыточных битов,
 * используемых для обнаружения и исправления ошибок.
 */
esp_err_t lora_set_cr(lora_coding_rate_t cr);

/*
 * Изменить рабочую частоту модуля (MHz).
 */
esp_err_t lora_set_freq(float mhz);

/*
 * Некоторые модули предоставляют готовые профили передачи данных
 * (Air Data Rate), представляющие собой наборы параметров SF, BW и CR.
 *
 * Функция переключает модуль на указанный профиль,
 * если такая возможность поддерживается.
 */
esp_err_t lora_set_level_air_rate(int level);

/*
Базовая конфигурация модуля
*/
esp_err_t lora_configure();


#ifdef __cplusplus
}
#endif

#endif
