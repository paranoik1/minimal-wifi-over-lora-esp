#include "driver/uart.h"
#include "transport_uart.h"
#include "freertos/projdefs.h"
#include <stdint.h>



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


size_t receive_packet(uint8_t *buffer, size_t max_len) {
    memset(buffer, 0, max_len);

    int length = uart_read_bytes(UART_SERVER, buffer, max_len, pdMS_TO_TICKS(20));
    if (length == -1) return 0;

    return length;
}


void send_packet(const uint8_t *packet, size_t size_packet) {
    printf("size: %d\n", size_packet);
    int bytes_written = uart_write_bytes(UART_SERVER, packet, size_packet);
    if (bytes_written == -1) {
        printf("Error: Failed to write UART\n");
    } 
}
