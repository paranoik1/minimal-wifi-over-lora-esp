# main/CMakeLists.txt
idf_component_register(
    SRCS "transport_uart.c" "main.cpp" "BLEManager.cpp"
    INCLUDE_DIRS "."
    REQUIRES arduino-esp32
)

# Отключаем warning о неинициализированных полях структур
# Это проблема в arduino-esp32, а не в вашем коде
#target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-missing-field-initializers)
#add_compile_options(-Wno-missing-field-initializers)
