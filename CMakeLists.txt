set(COMPONENT_REQUIRES ${COMPONENT_REQUIRES} "bt" "esp32-abseil-cpp" "esp32-utilities")
set(COMPONENT_SRCS "ble/ble_server.cpp" "ble/ble_profile.cpp" "ble/uuid.cpp"
                   "ble/ble_service.cpp" "ble/ble_characteristic.cpp" "ble/ble_value.cpp")
set(COMPONENT_ADD_INCLUDEDIRS ".")
set(COMPONENT_PRIV_INCLUDEDIRS "ble")

register_component()
