set(COMPONENT_REQUIRES ${COMPONENT_REQUIRES} "bt" "esp32-abseil-cpp" "esp32-utilities")
set(COMPONENT_SRCS "ble_server.cpp" "ble_profile.cpp" "uuid.cpp"
                   "ble_service.cpp" "ble_characteristic.cpp" "ble_value.cpp")
set(COMPONENT_ADD_INCLUDEDIRS ".")

register_component()
