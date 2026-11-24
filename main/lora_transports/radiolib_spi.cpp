#include <cstdio>

#include "SPI.h"
#include "esp_attr.h"
#include "esp_err.h"
#include "packet.h"
#include "RadioLib.h"
#include "lora_transport.h"

// Пины, которые вы определили
#define NSS         5
#define DIO1        13
#define NRST        15
#define BUSY        14
#define SPI_SCK     18
#define SPI_MISO    19
#define SPI_MOSI    23

#define TAG         "transport"


struct {
    uint8_t power;
    uint8_t sf;
    float bw;                 // Полоса пропускания, кГц
    lora_coding_rate_t cr;    // Кодовый коэффициент, например 5 для 4/5
    float freq_mhz;           // Частота, МГц
} last_config = {};


static Module* mod = new Module(NSS, DIO1, NRST, BUSY, SPI);
static SX1262 radio(mod);

static volatile int edges = 0;
static volatile bool operation_done = true;

static void IRAM_ATTR irq_isr() {
    edges += 1;
    operation_done = true;
}


esp_err_t lora_init()
{
    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, NSS);
    
    int state = radio.begin();
    if (state != RADIOLIB_ERR_NONE) 
        return ESP_FAIL;

    radio.setDio2AsRfSwitch(true);
    radio.setSyncWord(0x12, 0x24); 
    // Тоже самое, что и setDio1Action, только метод ниже определен для всех лора модулей (SX127x и тд)
    // Тогда как setDio1Action только для SX126x (является оберткой над setIrqAction)
    radio.setIrqAction(irq_isr);
    radio.setPreambleLength(8);
    radio.setCRC(1);

    lora_set_bw(250);
    lora_set_freq(433.10);
    lora_set_sf(11);
    lora_set_power(10);
    lora_set_cr(LORA_CR_4_7);

    radio.startReceive();

    return ESP_OK;
}



int get_transport_edges()
{
    return edges;
}

Packet* transport_receive_packet(RSSI* rssi)
{
    if (!operation_done) return NULL;
    operation_done = false;

    uint8_t pkt_raw[MAX_PACKET_SIZE] = {0};
    int status = radio.readData(pkt_raw, sizeof(pkt_raw));
    if (status != RADIOLIB_ERR_NONE) {
        ESP_LOGE(TAG, "Произошла ошибка чтения пакета: %d", status);
        return NULL;
    }

    radio.startReceive();
    return packet_from_bytes(pkt_raw);
}

bool transport_send_packet(Packet* packet)
{
    uint8_t pkt_raw[MAX_PACKET_SIZE] = {0};
    size_t pkt_size = 0;
    packet_to_bytes(packet, pkt_raw, &pkt_size);

    operation_done = false;

    int status = radio.transmit(pkt_raw, pkt_size);
    radio.startReceive();

    if (status != RADIOLIB_ERR_NONE) {
        ESP_LOGE(TAG, "Получен код ошибки при отправке пакета: %d", status);
        return false;
    }

    return true;
}


static esp_err_t radiolib_to_esp_err(int status_code) {
    switch (status_code) {
        case RADIOLIB_ERR_NONE:
            return ESP_OK;
        default:
            return ESP_FAIL;
    }
}


esp_err_t lora_set_power(uint8_t power)
{
    esp_err_t err = radiolib_to_esp_err(radio.setOutputPower(power));
    if (err == ESP_OK) {
        last_config.power = power;
    }
    return err;
}

esp_err_t lora_set_sf(uint8_t sf)
{
    esp_err_t err = radiolib_to_esp_err(radio.setSpreadingFactor(sf));
    if (err == ESP_OK) {
        last_config.sf = sf;
    }
    return err;
}

esp_err_t lora_set_bw(float bw)
{
    esp_err_t err = radiolib_to_esp_err(radio.setBandwidth(bw));
    if (err == ESP_OK) {
        last_config.bw = bw;
    }
    return err;
}

esp_err_t lora_set_cr(lora_coding_rate_t cr)
{
    esp_err_t err = radiolib_to_esp_err(radio.setCodingRate(cr));
    if (err == ESP_OK) {
        last_config.cr = cr;
    }
    return err;
}

esp_err_t lora_set_freq(float mhz)
{
    esp_err_t err = radiolib_to_esp_err(radio.setFrequency(mhz));
    if (err == ESP_OK) {
        last_config.freq_mhz = mhz;
    }
    return err;
}


esp_err_t lora_set_level_air_rate(int level)
{
    (void)level;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t lora_start_settings_mode()
{
    return ESP_OK;
}

esp_err_t lora_apply_settings()
{
    return ESP_OK;
}


esp_err_t lora_get_config(char* help, size_t buffer_size)
{
    if (help == nullptr || buffer_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    const int written = std::snprintf(
        help,
        buffer_size,
        "Power: %u dBm\n"
        "SF: %u\n"
        "BW: %.1f kHz\n"
        "CR: 4/%u\n"
        "Frequency: %.2f MHz\n",
        last_config.power,
        last_config.sf,
        last_config.bw,
        last_config.cr,
        last_config.freq_mhz
    );

    if (written < 0) {
        help[0] = '\0';
        return ESP_FAIL;
    }

    if (written >= buffer_size) {
        // snprintf оставляет строку с нулевым завершением,
        // но содержимое будет обрезано.
        return ESP_ERR_INVALID_SIZE;
    }

    return ESP_OK;
}

