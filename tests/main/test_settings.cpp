#include <stdio.h>
#include "lora_transport.h"
#include "unity.h"
#include "settings_lora.hpp"


struct fake_lora_t {
    int sf_calls = 0;
    int power_calls = 0;
    int bw_calls = 0;
    int cr_calls = 0;
    int freq_calls = 0;
    int level_air_rate_calls = 0;

    uint8_t last_sf = 0;
    uint8_t last_power = 0;
    float last_bw = 0.0f;
    lora_coding_rate_t last_cr = LORA_CR_4_5;
    float last_freq = 0.0f;
    int last_level_air_rate = 0;
};

fake_lora_t fake_lora;

esp_err_t lora_set_power(uint8_t power)
{
    ++fake_lora.power_calls;
    fake_lora.last_power = power;
    return ESP_OK;
}

esp_err_t lora_set_sf(uint8_t sf)
{
    ++fake_lora.sf_calls;
    fake_lora.last_sf = sf;
    return ESP_OK;
}

esp_err_t lora_set_bw(float bw)
{
    ++fake_lora.bw_calls;
    fake_lora.last_bw = bw;
    return ESP_OK;
}

esp_err_t lora_set_cr(lora_coding_rate_t cr)
{
    ++fake_lora.cr_calls;
    fake_lora.last_cr = cr;
    return ESP_OK;
}

esp_err_t lora_set_freq(float mhz)
{
    ++fake_lora.freq_calls;
    fake_lora.last_freq = mhz;
    return ESP_OK;
}

esp_err_t lora_set_level_air_rate(int level)
{
    ++fake_lora.level_air_rate_calls;
    fake_lora.last_level_air_rate = level;
    return ESP_OK;
}



TEST_CASE("apply_setting: valid params", "[settings_lora]") {
    fake_lora = {};

    TEST_ASSERT_EQUAL(ESP_OK, apply_setting("SF", "12"));
    TEST_ASSERT_EQUAL(ESP_OK, apply_setting("POWER", "17"));
    TEST_ASSERT_EQUAL(ESP_OK, apply_setting("BW", "125"));
    TEST_ASSERT_EQUAL(ESP_OK, apply_setting("CR", "5"));
    TEST_ASSERT_EQUAL(ESP_OK, apply_setting("FREQ", "915.15"));
    TEST_ASSERT_EQUAL(ESP_OK, apply_setting("LEVEL_AIR_RATE", "2"));

    TEST_ASSERT_EQUAL(1, fake_lora.sf_calls);
    TEST_ASSERT_EQUAL(12, fake_lora.last_sf);

    TEST_ASSERT_EQUAL(1, fake_lora.power_calls);
    TEST_ASSERT_EQUAL(17, fake_lora.last_power);

    TEST_ASSERT_EQUAL(1, fake_lora.bw_calls);
    TEST_ASSERT_EQUAL_FLOAT(125.0f, fake_lora.last_bw);

    TEST_ASSERT_EQUAL(1, fake_lora.cr_calls);
    TEST_ASSERT_EQUAL(LORA_CR_4_5, fake_lora.last_cr);

    TEST_ASSERT_EQUAL(1, fake_lora.freq_calls);
    TEST_ASSERT_EQUAL_FLOAT(915.15f, fake_lora.last_freq);

    TEST_ASSERT_EQUAL(1, fake_lora.level_air_rate_calls);
    TEST_ASSERT_EQUAL(2, fake_lora.last_level_air_rate);
}

TEST_CASE("apply_setting: invalid params", "[settings_lora]") {
    fake_lora = {};

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, apply_setting("SdsF", "12"));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, apply_setting("POWsdcER", "17"));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, apply_setting("BdcW", "125"));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, apply_setting("CaR", "5"));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, apply_setting("FREcQ", "915.15"));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, apply_setting("LEVacsEL_AIR_RATE", "1"));

    TEST_ASSERT_EQUAL(0, fake_lora.sf_calls);
    TEST_ASSERT_EQUAL(0, fake_lora.power_calls);
    TEST_ASSERT_EQUAL(0, fake_lora.bw_calls);
    TEST_ASSERT_EQUAL(0, fake_lora.cr_calls);
    TEST_ASSERT_EQUAL(0, fake_lora.freq_calls);
    TEST_ASSERT_EQUAL(0, fake_lora.level_air_rate_calls);
}


TEST_CASE("apply_settings_list: valid settings", "[settings_lora]") {
    fake_lora = {};

    char notify_message[300];
    printf("sizeof(notify_message) = %zu\n", sizeof(notify_message));
    esp_err_t err = apply_settings_list(String("SF=12\nPOWER=10\nCR=7\nFREQ=900.15\nBW=65.50\n"), notify_message, sizeof(notify_message));
    printf("message = %s", notify_message);

    TEST_ASSERT_EQUAL(ESP_OK, err);

    TEST_ASSERT_EQUAL(1, fake_lora.sf_calls);
    TEST_ASSERT_EQUAL(1, fake_lora.power_calls);
    TEST_ASSERT_EQUAL(1, fake_lora.cr_calls);
    TEST_ASSERT_EQUAL(1, fake_lora.freq_calls);
    TEST_ASSERT_EQUAL(0, fake_lora.level_air_rate_calls);
    TEST_ASSERT_EQUAL(1, fake_lora.bw_calls);

    TEST_ASSERT_EQUAL(12, fake_lora.last_sf);
    TEST_ASSERT_EQUAL(10, fake_lora.last_power);
    TEST_ASSERT_EQUAL(LORA_CR_4_7, fake_lora.last_cr);
    TEST_ASSERT_EQUAL(900.15, fake_lora.last_freq);
    TEST_ASSERT_EQUAL_FLOAT(65.50, fake_lora.last_bw);
}

extern "C" void app_main() {
    unity_run_menu();
}
