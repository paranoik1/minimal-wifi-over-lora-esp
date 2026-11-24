# Minimal Wi-Fi over LoRa — ESP32 Client

> Этот репозиторий содержит прошивку клиентского узла на базе ESP32 и является одним из компонентов общей системы. Описание архитектуры, протокола и остальных компонентов находится в [основном репозитории](https://github.com/paranoik1/Minimal_Wi-Fi_over_Lora).

ESP32 Client выступает промежуточным узлом между [Android-приложением](https://github.com/paranoik1/minimal-wifi-over-lora-android) и [Linux Server](https://github.com/paranoik1/minimal-wifi-over-lora-server). Отвечает за приём и передачу данных между BLE и LoRa, а также за первичную проверку получаемых пакетов.

## Реализовано

- **BLE-коммуникация** — передача полученных от сервера данных в Android-приложение через BLE Notifications.
- Получение пакетов от LoRa-модуля.
- Отправка пакетов по LoRa
- **Проверка пакетов** — проверка контрольных сумм полученных пакетов перед дальнейшей обработкой.
- **Настройка LoRa** — написаны методы для конфигурация LoRa модулей при старте.
- Возможность динамически менять данные LoRa модуля (SF/BW/CR и другие) через запись в соответствующую BLE характеристику
- Настройка LoRa модуля через изменение BLE характеристики

## Roadmap

- Добавить сохранение настроек LoRa модуля в Flash память и начинать инициализацию LoRa при следующем запуске с их чтения и применения
- Добавить кнопку на устройство для повторного запуска Advertising (рекламы BLE для повторного подключения устройств к телефонам).
- Устранить проблемы с отключением клиента при первом подключении.

- ~~**Синхронизация настроек LoRa** — согласование параметров радиоканала с [Linux Server](https://github.com/paranoik1/minimal-wifi-over-lora-server).~~ - пришел к выводу, что это не реализуемо в многопользовательской сети с одним LoRa модулем, так как у серверного модуля должны совпадать настройки со всеми клиентскими модулями, дабы успешно принимать и отправлять данные им 
    - учитывая, что отправка и приемка данных может осуществляться параллельно, сервер не может динамически менять данные LoRa модуля из за одного клиента
    - необходимо будет найти золотую середину SF/BW/CR (Spreading Factor, Bandwidth, Coding Rate)

```markdown
## Компиляция

### Требования

- ESP-IDF v6.x (проект собран и протестирован на v6.1.1)
- Python 3.10+ (требование ESP-IDF)
- Драйверы USB-UART для вашей платы ESP32 (CP210x, CH340 и т.п.)

### 1. Клонирование репозитория

```bash
git clone https://github.com/paranoik1/Minimal_Wi-Fi_over_LoRa-esp-client.git
cd Minimal_Wi-Fi_over_LoRa-esp-client
```

### 2. Установка RadioLib

Склонируйте библиотеку в папку `components`:

```bash
git clone https://github.com/jgromes/RadioLib.git components/RadioLib
```

### 3. Правка `components/RadioLib/CMakeLists.txt`

Откройте файл и внесите три изменения.

Найдите строку:

```cmake
list(FILTER RADIOLIB_SOURCES EXCLUDE REGEX "src/hal/.*\\.cpp")
```

Сразу под ней добавьте:

```cmake
list(APPEND RADIOLIB_SOURCES "src/hal/Arduino/ArduinoHal.cpp")
```

В вызове `idf_component_register` добавьте `arduino-esp32` в конец `REQUIRES`:

```cmake
  REQUIRES esp_driver_gpio esp_driver_spi esp_driver_ledc esp_timer arduino-esp32
```

Сразу после `idf_component_register(...)` добавьте:

```cmake
target_compile_definitions(${COMPONENT_LIB} PUBLIC RADIOLIB_BUILD_ARDUINO)
```

Строка должна стоять именно после `idf_component_register`, иначе CMake упадёт с ошибкой `target_compile_definitions command is not scriptable`.

Итоговый файл должен выглядеть так:

```cmake
cmake_minimum_required(VERSION 3.13)

file(GLOB_RECURSE RADIOLIB_SOURCES
  "src/*.cpp"
)

list(FILTER RADIOLIB_SOURCES EXCLUDE REGEX "src/hal/.*\\.cpp")
list(APPEND RADIOLIB_SOURCES "src/hal/Arduino/ArduinoHal.cpp")

if(ESP_PLATFORM)
  file(GLOB RADIOLIB_ESP_IDF_HAL_SOURCES "src/hal/ESP-IDF/*.cpp")

  idf_component_register(
    SRCS ${RADIOLIB_SOURCES} ${RADIOLIB_ESP_IDF_HAL_SOURCES}
    INCLUDE_DIRS . src
    REQUIRES esp_driver_gpio esp_driver_spi esp_driver_ledc esp_timer arduino-esp32
  )

  target_compile_definitions(${COMPONENT_LIB} PUBLIC RADIOLIB_BUILD_ARDUINO)

  return()
endif()

if(CMAKE_SCRIPT_MODE_FILE)
  message(FATAL_ERROR "Attempted to build RadioLib in script mode")
endif()

project(radiolib)

add_library(RadioLib ${RADIOLIB_SOURCES})

target_include_directories(RadioLib
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
           $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

set_property(TARGET RadioLib PROPERTY CXX_STANDARD 20)

target_compile_options(RadioLib PRIVATE -Wall -Wextra -Wpedantic -Wdouble-promotion)

include(GNUInstallDirs)

install(TARGETS RadioLib
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/RadioLib
  FILES_MATCHING PATTERN "*.h"
)
```

### 4. Настройка и сборка

```bash
. $IDF_PATH/export.sh
idf.py set-target esp32
idf.py menuconfig    # включите CONFIG_LORA_TRANSPORT_SPI или CONFIG_LORA_TRANSPORT_UART
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```
```

## Технологии

- C / C++
- ESP-IDF
- ESP32
- Bluetooth Low Energy (BLE)
- UART
- LoRa
