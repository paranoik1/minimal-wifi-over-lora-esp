#include "driver/uart.h"
#include "transport_uart.h"
#include "freertos/projdefs.h"
#include <machine/endian.h>
#include <stdint.h>
#include "packet.h"
#include <endian.h>



// UART_NUM_0 нужен для отладочной консоли (для Serial.println, printf, cout и другое)
// поэтому его использовать для передачи данных в другие микроконтроллеры нестоит 
// (можно, но тогда теряем отладочную консоль)
#define UART_SERVER         UART_NUM_1
#define UART_BUFFER_SIZE    2048
#define RX_GPIO             16
#define TX_GPIO             17


void init_transport() {
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 122,
    };
    ESP_ERROR_CHECK(
        uart_param_config(UART_SERVER, &uart_config)
    );
    // Более явно (tx и rx соответствуют пинам по умолчанию для UART_NUM_1)
    uart_set_pin(
        UART_SERVER, 
        TX_GPIO, 
        RX_GPIO, 
        UART_PIN_NO_CHANGE, 
        UART_PIN_NO_CHANGE
    );

    ESP_ERROR_CHECK(
        uart_driver_install(
            UART_SERVER, 
            UART_BUFFER_SIZE, UART_BUFFER_SIZE, 
            0, NULL, 0
        )
    );
}


Packet* transport_receive_packet() {
    uint8_t pkt_raw[MAX_PACKET_SIZE] = {};

    // Сначала читаем метаданные.
    int length = uart_read_bytes(
        UART_SERVER,
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
        UART_SERVER,
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

    printf("Размер пакета: %d\n", size_packet);
    int bytes_written = uart_write_bytes(UART_SERVER, pkt_raw, size_packet);
    
    free(pkt_raw);

    if (bytes_written == -1) {
        printf("Error: Failed to write UART\n");
    } 
    printf("Bytes written: %d\n", bytes_written);
}
