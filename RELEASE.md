# Прошивка

## Файлы

К релизу приложены образы прошивок в формате `.elf`. Какой образ для какой платы
и что он делает — см. таблицу «Products» и раздел «Firmware images» в README.md.

## Подключение

См. [здесь](https://alicelarp.atlassian.net/wiki/spaces/HW/pages/790528004/OpenOCD#%D0%9F%D0%BE%D0%B4%D0%BA%D0%BB%D1%8E%D1%87%D0%B5%D0%BD%D0%B8%D0%B5-%D0%BF%D1%80%D0%BE%D0%B3%D1%80%D0%B0%D0%BC%D0%BC%D0%B0%D1%82%D0%BE%D1%80%D0%B0)
информацию о том, что куда подключать.

## Black Magic Probe

Нужен GDB для ARM (`gdb-multiarch` в Linux или `arm-none-eabi-gdb`). Black Magic Probe
создаёт два последовательных порта, GDB-сервер — на первом из них (в Linux обычно
`/dev/ttyACM0`, в Windows — `COMx`; для двузначных номеров пишется `\\.\COM11`).

```bash
gdb-multiarch -nx --batch \
  -ex 'target extended-remote /dev/ttyACM0' \
  -ex 'monitor swdp_scan' -ex 'attach 1' \
  -ex 'load' -ex 'compare-sections' -ex 'kill' \
  firefly.elf
```

Это те же команды, которыми прошивает CI.

## nRF-девкит или J-Link (nRF Connect Programmer)

Programmer принимает только `.hex`, поэтому сначала конвертируем:

```bash
arm-none-eabi-objcopy -O ihex firefly.elf firefly.hex
```

Устанавливаем [nRF Connect for Desktop](https://www.nordicsemi.com/Products/Development-tools/nrf-connect-for-desktop),
в нём — Programmer. Выбираем в выпадающем списке девкит или J-Link и прошиваем
полученным hex-файлом.
