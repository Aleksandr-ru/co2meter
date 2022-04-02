# DIY CO2 meter and grapher for home and office use

![CO2 meter](https://aleksandr.ru/sitefiles/338/P20223-151350.jpg)

## Features

- display current CO2 level, temperature and humidity
- display historical graph of CO2 level for last 30 minutes
- switch to large display of CO2 level
- alarm when CO2 exceeds limit of 1000 ppm
- self calibration with countdown display

## Components

- Arduino nano
- 0.96" OLED display
- MQ135 sensor
- DHT11 sensor
- 6x6x4.3mm button
- [3D printed case](https://www.thingiverse.com/thing:5259637)
- Screws

## Schematics

Common VCC - arduino 5v pin, common GND - arduino GND pin.

Display connection: SDA to A4 pin, SCK to A5 pin, VDD to 3v3 pin, GND to common GND.

MQ135 connection: A0 to A0 pin, VCC to common VCC pin, GND to common GND pin.

DHT11 connection: OUT to D2 pin, "+" to common VCC pin, "-" to common GND pin.

Button: connection: to D2 pin and GND pin.

## Assembly

See instructions at my website [in Russian](https://aleksandr.ru/blog/domashniy_co2_metr) or [translated to English](https://aleksandr-ru.translate.goog/blog/domashniy_co2_metr?_x_tr_sl=ru&_x_tr_tl=en&_x_tr_hl=ru&_x_tr_pto=wapp).
