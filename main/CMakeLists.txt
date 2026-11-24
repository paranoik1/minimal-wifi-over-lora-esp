# main/CMakeLists.txt
set(srcs
    "packet.c" 
    "main.cpp" 
    "BLEManager.cpp"
    "settings_lora.cpp"
)

set(requires
    arduino-esp32
    RadioLib
)

# set(priv_requires "")

if(CONFIG_LORA_TRANSPORT_UART)
    list(APPEND srcs "lora_transports/dx_smart.c")
endif()

if(CONFIG_LORA_TRANSPORT_SPI)
    list(APPEND srcs "lora_transports/radiolib_spi.cpp")
    # list(APPEND priv_requires RadioLib)
    # list(APPEND requires RadioLib)
endif()

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS "." "lora_transports"
    REQUIRES ${requires}
    # PRIV_REQUIRES ${priv_requires}
)

# Отключаем warning о неинициализированных полях структур
#target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-missing-field-initializers)
#add_compile_options(-Wno-missing-field-initializers)
