#include "esp_log.h"
#include "lora_transport.h"
#include "settings_lora.hpp"


esp_err_t apply_settings_list(String settings, char *notify_message, size_t max_size_message) {
    int msg_pos = 0;
    int pos = 0;

    while (pos < settings.length()) {
        if (msg_pos >= max_size_message) {
            return ESP_ERR_INVALID_SIZE;
        }
        int nl = settings.indexOf('\n', pos);
        if (nl < 0) continue;
        
        String line = settings.substring(pos, nl);
        
        pos = nl + 1;
        line.trim();

        int equal = line.indexOf('=', 0);
        if (equal <= 0) continue;

        char key[30] = {0};
        char value[20] = {0};
        line.substring(0, equal).toCharArray(key, sizeof(key));
        line.substring(equal + 1).toCharArray(value, sizeof(value));

        esp_err_t err = apply_setting(key, value);
        if (err == ESP_OK) {
            msg_pos += snprintf(
                &notify_message[msg_pos],
                sizeof(notify_message) - msg_pos, 
                "OK - %s = %s\n", 
                key, value
            );
        } else {
            ESP_LOGE("client_configure_lora", "Ошибка применения настроек %s = %s: %s", key, value, esp_err_to_name(err));

            msg_pos += snprintf(
                &notify_message[msg_pos], 
                sizeof(notify_message) - msg_pos, 
                "ERROR - %s = %s: %s\n",
                key, value, esp_err_to_name(err)
            );
        }
    }

    return ESP_OK;
}



esp_err_t apply_setting(const char *key, const char *value) {
    const char* tag = "apply_settings";

    if (strcmp(key, "SF") == 0) {
        uint8_t sf = (uint8_t)atoi(value);
        return lora_set_sf(sf);
    } 
    
    if (strcmp(key, "POWER") == 0) {
        uint8_t power = (uint8_t)atoi(value);
        return lora_set_power(power);
    } 

    if (strcmp(key, "CR") == 0) {
        lora_coding_rate_t cr = (lora_coding_rate_t)atoi(value);
        return lora_set_cr(cr);
    }

    if (strcmp(key, "LEVEL_AIR_RATE") == 0) {
        int level = atoi(value);
        return lora_set_level_air_rate(level);
    }
    
    if (strcmp(key, "BW") == 0) {
        char *err;
        float bw = strtof(value, &err);
        if (err == value) {
            ESP_LOGE(tag, "Ошбика при конвертации bw = '%s' в float", value);
            return ESP_FAIL;
        }
        return lora_set_bw(bw);
    } 
    
    if (strcmp(key, "FREQ") == 0) {
        char *err;
        float freq = strtof(value, &err);
        if (err == value) {
            ESP_LOGE(tag, "Ошбика при конвертации freq = '%s' в float", value);
            return ESP_FAIL;
        }
        return lora_set_freq(freq);
    } 

    return ESP_ERR_INVALID_ARG;
}
