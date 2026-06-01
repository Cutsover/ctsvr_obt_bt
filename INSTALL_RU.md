# Установка и прошивка

1. Распакуйте проект в `D:\ESP_Projects\ctsr_obd_reader`.
2. Откройте ESP-IDF Terminal.
3. Выполните:

```powershell
cd D:\ESP_Projects\ctsr_obd_reader
idf.py set-target esp32
idf.py menuconfig
```

В `menuconfig` проверьте:

- Serial flasher config → Flash size → 4 MB
- Component config → Bluetooth → Bluetooth enabled
- Bluetooth Host → Bluedroid
- Classic Bluetooth enabled
- BLE enabled
- SPP enabled
- Controller mode → BTDM / Dual Mode

Затем:

```powershell
idf.py build
idf.py -p COM8 flash monitor
```

Если COM-порт другой, замените `COM8`.
