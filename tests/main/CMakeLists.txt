idf_component_register(SRCS 
                        "test_settings.cpp"
                        "../../main/settings_lora.cpp"
                    INCLUDE_DIRS
                        "."
                        "../../main/lora_transports"
                        "../../main/"
                    REQUIRES 
                        unity arduino-esp32)
