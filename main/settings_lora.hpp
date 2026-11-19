#pragma once

#include "WString.h"
#include "esp_err.h"
#include <cstddef>


esp_err_t apply_setting(const char *key, const char *value);

esp_err_t apply_settings_list(String settings, char *notify_message, size_t max_size_message) ;
