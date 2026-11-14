#include <driver/gpio.h>
#include <driver/uart.h>
#include <hal/gpio_types.h>

#include "packet.h"
#include "lora_transport.h"


// UART_NUM_0 нужен для отладочной консоли (для Serial.println, printf, cout и другое)
// поэтому его использовать для передачи данных в другие микроконтроллеры нестоит 
// (можно, но тогда теряем отладочную консоль)
#define LORA_UART           UART_NUM_1
#define LORA_BOUD           9600
#define UART_BUFFER_SIZE    2048
#define RX_GPIO             16
#define TX_GPIO             17
#define AUX_GPIO            23
#define LOG_TAG             "transport"


static volatile uint32_t aux_edges   = 0;   // счётчик фронтов (события!)
static volatile uint32_t aux_level = 0;



// ISR - INterrupt Service Routine - Процедура обработки прерываний
static void IRAM_ATTR aux_isr(void *) {
    aux_level = gpio_get_level(AUX_GPIO);
    aux_edges++;
}


int get_transport_edges() {
    return aux_edges;
}


int get_transport_state() {
    return aux_level;
}


void init_transport() {
    uart_config_t uart_config = {
        .baud_rate = LORA_BOUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    ESP_ERROR_CHECK(
        uart_param_config(LORA_UART, &uart_config)
    );
    // Более явно (tx и rx соответствуют пинам по умолчанию для UART_NUM_1)
    uart_set_pin(
        LORA_UART, 
        TX_GPIO, 
        RX_GPIO, 
        UART_PIN_NO_CHANGE, 
        UART_PIN_NO_CHANGE
    );

    ESP_ERROR_CHECK(
        uart_driver_install(
            LORA_UART, 
            UART_BUFFER_SIZE, UART_BUFFER_SIZE, 
            0, NULL, 0
        )
    );

    uart_flush(LORA_UART);
    gpio_config_t c = {
        .pin_bit_mask = 1ULL << AUX_GPIO,
        .mode         = GPIO_MODE_INPUT,
        // Два параметра ниже нужны для того, чтобы удерживать определенный уровень контакта (LOW, HIGH)
        // в случае, когда к GPIO ничего не подключено (если они оба DISABLE -> будет непредсказуемое состояние пина - то LOW, то HIGH)
        // Под копотом - в зависимости от выбора PULLUP или PULLDOWN подключается к цепи резистор с большим сопртоивлением, который ведет либо к VCС, либо к GND соответственно
        // Удерживает уровень контакта в HIGH, если к пину ничего не подключено (DISABLE - не удерживает)
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        // Удерживает уровень контакта в LOW, если к пину ничего не подключено (ENABLE - удерживает)
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type    = GPIO_INTR_ANYEDGE,
    };
    gpio_config(&c);

    // Включение глобального обработчика прерываний Interrupt (который прохоиться по зарегистрированным обработчикам GPIO и вызывает их при надобности)
    gpio_install_isr_service(0);
    // Установка обработчика для конкретного GPIO
    gpio_isr_handler_add(AUX_GPIO, aux_isr, NULL);

    ESP_LOGI("AUX", "init: level=%d", gpio_get_level(AUX_GPIO));
}

Packet* transport_receive_packet() {
    uint8_t pkt_raw[MAX_PACKET_SIZE] = {};

    // Сначала читаем метаданные.
    int length = uart_read_bytes(
        LORA_UART,
        pkt_raw,
        SIZE_METADATA_PACKET,
        pdMS_TO_TICKS(20)
    );

    if (length != SIZE_METADATA_PACKET) {
        return NULL;
    }

    size_t payload_length = pkt_raw[SIZE_METADATA_PACKET - 1];
    size_t remaining_length = payload_length + BYTES_AFTER_PAYLOAD; // payload + CRC

    // так как каждый элемент массива pkt_raw = 1 байту => для получения его длины достаточно лишь sizeof (без деления на размер 1 элемента)
    if (SIZE_METADATA_PACKET + remaining_length > sizeof(pkt_raw)) {
        return NULL;
    }

    // Читаем payload и CRC в тот же буфер.
    length = uart_read_bytes(
        LORA_UART,
        // Вспоминаем адресную арифметику - pkt_raw - указатель на первый элемент в массиве
        // => адрес 1 элемента + SIZE_METADATA_PACKET (5) будет указывать на 5 элемент в массиве => с него и начнется запись данных до адрес 1 эл + SIZE_METADATA_PACKET + remaining_length
        pkt_raw + SIZE_METADATA_PACKET,
        remaining_length,
        pdMS_TO_TICKS(20)
    );

    if (length != remaining_length) {
        printf("[transport_receive] не прошел проверку длины: length = %d, remaining_length = %d\n", length, remaining_length);
        return NULL;
    }

    return packet_from_bytes(pkt_raw);
}


void transport_send_packet(Packet *packet) {
    size_t size_packet;
    uint8_t* pkt_raw = packet_to_bytes(packet, &size_packet);

    ESP_LOGD(LOG_TAG, "Размер пакета: %d\n", size_packet);
    int bytes_written = uart_write_bytes(LORA_UART, pkt_raw, size_packet);
    
    free(pkt_raw);

    if (bytes_written == -1) {
        ESP_LOGE(LOG_TAG, "Error: Failed to write UART\n");
    } 
    ESP_LOGD(LOG_TAG, "Bytes written: %d", bytes_written);
}


// void at_entry() {
//     uart_write_bytes(LORA_UART, "+++", 3);
//     char* buff = NULL;
//     uart_read_bytes(LORA_UART, buff, uint32_t length, uint32_t ticks_to_wait)
// }


// void at_exit() {

// }



// void at_set() {

// }



// void lora_configure()
