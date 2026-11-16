/*
UART tranposrt для DX-SMART Lora модуля - DX-LR32 900T22D
*/

#include <driver/gpio.h>
#include <driver/uart.h>
#include <hal/gpio_types.h>
#include <math.h>
#include <string.h>

#include "esp_err.h"
#include "esp_timer.h"
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


static volatile uint32_t aux_edges = 0;
static volatile uint32_t aux_level = 0;



// ISR - INterrupt Service Routine - Процедура обработки прерываний
/*
Из документации к модулю:
- Когда модуль обнаружит полученные данные, AUX выведет высокий уровень 2-3ms заранее,
который используется для того, чтобы побудить внешний MCU 
(микроконтроллер) быть готовым к получению данных

- aux_level = HIGH => данные передаются или получаются, модуля занят

- aux_level = LOW => успешно данные переданы или получены, модуль не занят
*/
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

    gpio_config_t rx = {
        .pin_bit_mask = 1ULL << RX_GPIO,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&rx);

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

// Только для DX-SMART LR32 при AT+DRSSI1
static int8_t dx_smart_rssi_to_dbm(uint8_t raw_byte) {
    return -(int8_t)(0xFF - raw_byte);
}


// FIXME: осуществлять чтение всего буфера без таймаутов и только в том случае, если модуль не занят
Packet* transport_receive_packet(int8_t* rssi) {
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
    size_t packet_length = SIZE_METADATA_PACKET + remaining_length;

    // так как каждый элемент массива pkt_raw = 1 байту => для 
    // получения его длины достаточно лишь sizeof (без деления на размер 1 элемента)
    if (packet_length > sizeof(pkt_raw)) {
        ESP_LOGE(LOG_TAG, 
            ">> received - packet_length (len=%d) превышает размер pkt_raw (%d)", 
            packet_length, sizeof(pkt_raw)
        );
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
        ESP_LOGE(LOG_TAG, 
            ">> receive - не прошел проверку длины: length = %d, remaining_length = %d", 
            length, remaining_length
        );
        return NULL;
    }

    uint8_t raw_rssi = 0;
    length = uart_read_bytes(
        LORA_UART, 
        &raw_rssi, 
        1, 
        pdMS_TO_TICKS(20)
    );
    if (length == 0) {
        ESP_LOGI(LOG_TAG, ">> receive - нет информации об rssi");
    } else if (length < 0) {
        ESP_LOGE(LOG_TAG, ">> receive - ошибка при чтении rssi");
    } else {
        *rssi = dx_smart_rssi_to_dbm(raw_rssi);
        ESP_LOGI(LOG_TAG, "RSSI: %d", *rssi);
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
        return;
    } 
    
    ESP_LOGD(LOG_TAG, "Bytes written: %d", bytes_written);
}


/* 
читаем одну строку без \r\n 

Возвращает длину прочитанной строки
`-1` - непредвиденная ошибка чтения
`-2` - превышен максимальный размер `buf` (`max` переменная)

При `-2` в буфере находиться строка без конечного символа `\0`
*/
static int at_line(char *buf, size_t max, int to_ms)
{
    size_t len = 0; 
    int64_t t0 = esp_timer_get_time();

    while ((esp_timer_get_time() - t0) < to_ms * 1000) {
        uint8_t c;
        int read_bytes = uart_read_bytes(
            LORA_UART, &c, 1, 
            pdMS_TO_TICKS(20)
        );

        // Если read_bytes <= 0, значит по истечению таймаута не было получено ни одного байта (0)
        // Или же возникла ошибка (-1)
        if (read_bytes <= 0) 
            continue;

        // Если len был уже увеличен, значит это конец строки => возвращаем длину
        // В ином случае (все еще len = 0) мы получали что то иное => продолжаем итерацию
        if (c == '\r' || c == '\n') { 
            if (!len) 
                continue; 
            break;
        }

        // Если текущая длина строки в буфере не превышает максимального индекса этого буффера
        // То записываем в len значение и инкрементируем его (увеличиваем на 1)
        if (len < max - 1) 
            buf[len++] = c;
        else {
            ESP_LOGD(
                LOG_TAG, 
                ">> at_line: превышена длина буфера - len=%d, max=%d, line=%s", 
                len, max, buf
            );
            return -2;
        }
    }

    buf[len] = 0;
    return len ? (int)len : -1;
}

static esp_err_t at_cmd(const char *cmd, char *resp, size_t resp_size, int to_ms)
{
    char line[192]; 
    int counter_lines = 0;
    esp_err_t ret = ESP_ERR_TIMEOUT;

    if (resp && resp_size) 
        resp[0] = 0;
    
    uart_flush_input(LORA_UART);
    uart_write_bytes(LORA_UART, cmd, strlen(cmd));
    uart_write_bytes(LORA_UART, "\r\n", 2);
    ESP_LOGD(LOG_TAG, ">> at_cmd: %s", cmd);

    int64_t t0 = esp_timer_get_time();
    while ((esp_timer_get_time() - t0) < (int64_t)to_ms * 1000) {
        int len_line = at_line(line, sizeof line, 200);
        if (len_line <= 0) 
            continue;

        counter_lines++;
        ESP_LOGD(LOG_TAG, "<< at_cmd: %s, count=%d", line, counter_lines);

        if (resp && resp_size) { 
            size_t u = strlen(resp);
            if (u < resp_size - 1) 
                snprintf(resp + u, resp_size - u, "%s\n", line); 
        }

        if (strstr(line, "EEROR") || strstr(line, "ERROR")) { 
            ret = ESP_FAIL; 
            break; 
        }

        if (strstr(line, "OK")) { 
            ret = ESP_OK;   
            break; 
        }

        // Для AT+HELP
        if (strstr(line, "========") && counter_lines > 3) {
            ret = ESP_OK;
            break;
        }
    }

    ESP_LOGD(LOG_TAG, "*? at_cmd: cmd = %s, time = %lld ms", 
        cmd, (esp_timer_get_time() - t0) / 1000);
    return ret;
}

/*
Возвращает 1, когда выходим из AT режима

0 - когда входим в AT режим

-1 - ошибка или непредвиденная ситуация
*/
static int at_mode_toggle() {
    uart_flush_input(LORA_UART);

    uart_write_bytes(LORA_UART, "+++\r\n", 5);
    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_LOGD(LOG_TAG, ">> at_mode_toggle");

    char r[128] = {};
    if (at_line(r, sizeof r, 500) <= 0) {
        ESP_LOGE(LOG_TAG, "<< at_mode_toggle: at_line вернул <= 0");
        return -1; 
    }

    ESP_LOGD(LOG_TAG, "response = %s\n", r);
    
    if (strcmp("Entry AT", r) == 0) 
        return 0;
    else if (strcmp("Exit AT", r) == 0) {
        char dummy[30] = {0};
        int counter = 0;
        int n;
        // - 1 под \0 символ
        while (counter < sizeof dummy - 1 && 
            (n = uart_read_bytes(
                LORA_UART, 
                &dummy[counter], 
                sizeof dummy - counter - 1, 
                pdMS_TO_TICKS(50)
            ) > 0)) 
        { 
            ESP_LOGD(LOG_TAG, "at_mode_toggle?: n = %d", n);
            counter += n;
        }

        ESP_LOGD(LOG_TAG, "*** clear bytes: %d, dummy=%s", counter, dummy);
        return 1;
    }

    ESP_LOGE(LOG_TAG, "at_mode - Непредвиденный ответ на +++ - %s", r);
    return -1;
}

static bool at_enter()
{
    int code = at_mode_toggle();
    // Если at_mode_toggle вызвал выход из AT режима - повторяем функцию at_enter
    if (code == 1) 
        return at_enter();
    // Если же произошла ошибка, возвращаем false
    else if (code < 0) 
        return false;

    // в ином случае все прошло успешно - возвращаем true
    return true;
}

/* выход: модуль сам перезагрузится */
static bool at_exit(void)
{
    // Тот же алгоритм, что и с at_enter
    int code = at_mode_toggle();
    if (code == 0) 
        return at_exit();
    else if (code < 0) 
        return false;

    return true;
}

/* сеттер, толерантный к синтаксису с '=' и без */
static esp_err_t at_set(const char *key, const char *val)
{
    char cmd[48], r[128];
    snprintf(cmd, sizeof cmd, "AT+%s%s", key, val);

    return at_cmd(cmd, r, sizeof r, 1000);
}

// static esp_err_t at_set_with_equal_char(const char *key, const char *val) {
//     char key_with_equal[30];
//     snprintf(key_with_equal, sizeof key_with_equal, "%s=", key);
//     return at_set(key_with_equal, val);
// }


/*
Функция для установки частоты у LoRa модуля

Особенности конкретно моего модуля:
- UART прошивка не позволяет точечно указать частоту
- Регулировать частоту модуля можно, указав нужный канал (AT+CHANNEL<HEX_CHANNEL>)
- Канал - это 16-ричное число, которое необходимо добавить к 850.15 (если переводить его в 10-чное число)
Пример: Чтобы установить 915.15 частоту необходимо:
- 910.15 - 850.15 = **60**
- 60 в 16-ричной системе = 3C
- Итоговая команда: AT_CHANNEL3C


Пределы канала в 10-чиной системе: от 1 до 98 включительно
*/
static esp_err_t lora_set_freq(double mhz)
{
    // lround возвращает long значение, в то время как round - double
    int n = (int)lround(mhz - 850.15);
    if (n < 0 || n > 99)
        return ESP_ERR_INVALID_ARG;
    char v[3]; 
    // %02X - 16-ричное число, минимум 2 знака (при недостатке забить нулями слева)
    // X - 16-ричные символы с заглавной буквы ставить
    // Пример: 60 -> 3C, Если бы было %02x -> 3c
    snprintf(v, sizeof v, "%02X", n);

    return at_set("CHANNEL", v);
}

// static void aux_wait_idle(int to_ms)
// {
//     int64_t t0 = esp_timer_get_time();
//     while (gpio_get_level(AUX_GPIO) && (esp_timer_get_time() - t0) < (int64_t)to_ms * 1000)
//         vTaskDelay(pdMS_TO_TICKS(5));
// }

esp_err_t lora_configure()
{
    if (!at_enter()) {
        ESP_LOGI(LOG_TAG, "Ошибка: at_enter вернул false");
        return ESP_FAIL;
    }
    ESP_LOGI(LOG_TAG, "В AT режиме");
    // at_set("BOUD",     "7");    // 115200
    at_set("MODE",    "0");        // transparent
    at_set("SLEEP",   "2");        // всегда в приёме
    at_set("LEVEL",   "2");        // SF11/BW500/CR4/5
    lora_set_freq(910.15);              // AT+CHANNEL41
    at_set("POWE",    "10");       // мозность - 10 для настольных тестов
    at_set("PACKET",  "3");        // Размер пакета - 230B
    at_set("DRSSI",   "1");        // RSSI последним байтом
    at_set("IQ",      "1");
    at_set("CRC",     "1");
    // DX-SMART модули имеют проприетарный способ шифрования связи на уровне модуля - его отключаю, чтобы была возможность работать с устройствами от разных поставщиков
    at_set("OPENKEY", "0");

    // at_cmd("AT+RESET", NULL, 0, 1, 1000);
    vTaskDelay(pdMS_TO_TICKS(800));

    char help[512];
    at_cmd("AT+HELP", help, sizeof help, 11500);
    ESP_LOGI(LOG_TAG, "CONFIG:\n%s", help);

    if (!at_exit()) {
        ESP_LOGE(LOG_TAG, "Ошибка: at_exit вернул false");
        return ESP_FAIL;
    }

    return ESP_OK;
}
