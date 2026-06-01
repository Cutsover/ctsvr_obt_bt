# CTSR OBD Reader Dual Scan

ESP-IDF проект для обычного ESP32 с Wi-Fi AP и двумя режимами поиска Bluetooth:

- Classic Bluetooth Scan / SPP — для ELM327 Bluetooth Classic.
- BLE Scan — для ELM327 Bluetooth Low Energy и других BLE-устройств.

Wi-Fi AP:

- SSID: `CTSR_Prod`
- Password: `elephant`
- URL: `http://10.10.10.1`

Classic SPP подключение и OBD-команды реализованы для Classic ELM327. BLE-часть пока диагностическая: она показывает BLE-устройства, чтобы определить, виден ли адаптер как BLE. После этого нужно смотреть UUID сервиса/характеристик конкретного адаптера.
