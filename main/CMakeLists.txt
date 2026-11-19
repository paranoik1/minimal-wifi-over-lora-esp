# main/CMakeLists.txt
idf_component_register(
    SRCS 
        "packet.c" 
        "main.cpp" 
        "BLEManager.cpp"
        "settings_lora.cpp"
        # Генератор выражения - нужен для вставки значения в конфигурацию еще до компиляции
        # if здесь вставить нельзя, так как SRCS секция принимает список, а не условную функцию
        # Синтаксис таков $< УСЛОВИЕ : ЗНАЧЕНИЕ_ЕСЛИ_ИСТИНА >
        # В данном случае у нас генератор в генераторе - 
        # преобразуем Kconfig значение в булевую переменную - $<BOOL:${CONFIG_LORA_TRANSPORT_UART}>
        # А затем, получив из прошлого выражения 0 или 1 получаем выражение - $<1 or 0 : lora_transports/uart.c>
        # если 1, все выражение заменяется ЗНАЧЕНИЕМ_ЕСЛИ_ИСТИНА, в ином случае - пустая строка
        # БЕЗ КОВЫЧЕК!! Генератор сам их подставит, в ином случае вы получите ошибку во время компиляции:
        # Cannot find source file: ...../main/"lora_transports/uart.c"
        $<$<BOOL:${CONFIG_LORA_TRANSPORT_UART}>:lora_transports/dx_lr32.c>
    INCLUDE_DIRS "." "lora_transports"
    REQUIRES arduino-esp32
)

# Отключаем warning о неинициализированных полях структур
#target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-missing-field-initializers)
#add_compile_options(-Wno-missing-field-initializers)
