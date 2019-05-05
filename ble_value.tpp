// Copyright (c) 2019 Zeyad Tamimi.  All rights reserved.

#ifndef COMPONENTS_BLE_BLE_VALUE_TPP
#define COMPONENTS_BLE_BLE_VALUE_TPP

template<typename T>
std::vector<uint8_t>
BLE_Value::default_serializer(T value)
{
    std::vector<uint8_t> serialized_value;
    serialized_value.resize(sizeof(T));
    std::reverse_copy(reinterpret_cast<uint8_t*>(&value),
                      reinterpret_cast<uint8_t*>(&value) + sizeof(T),
                      serialized_value.begin());
    return serialized_value;
}


template<typename T, typename=std::enable_if_t<std::conjunction_v<std::is_default_constructible<T>,
                                                                  std::is_integral<T>>>>
T
BLE_Value::default_deserializer(std::vector<uint8_t> serialized_value)
{
    T value = {};
    std::reverse_copy(serialized_value.begin(),
                      serialized_value.begin() + std::min(sizeof(T), serialized_value.size()),
                      reinterpret_cast<uint8_t*>(&value));
    return value;

}

template<typename T>
void
BLE_Value::value_set(T value, BLE_Value::Serializer<T> serializer)
{
    m_value = serializer(value);
}

template<typename T>
T
BLE_Value::value_get(BLE_Value::Deserializer<T> deserializer) const
{
    return deserializer(m_value);
}

#endif // COMPONENTS_BLE_BLE_VALUE_TPP

