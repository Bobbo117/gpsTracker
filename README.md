# gpsTracker

 Tracks location on street map via cellular IoT
 
![Screenshot_20230607-152750_Chrome](https://github.com/Bobbo117/gpsTracker/assets/58577175/db9bc860-5f36-4b81-a104-12078f6f50c4)


<br>

## How It Works

An inexpensive IoT SIM card gains access to the cloud via a cellular connection.

https://www.hologram.io/

<br>

## Tested With

-   [**LiLLYGO** `SIM7000G` board with 
    integrated **ESP32** Wrover](https://www.amazon.com/LILYGO-Development-ESP32-WROVER-B-Battery-T-SIM7000G/dp/B099RQ7BSR)
    
    ![20230607_105519](https://github.com/Bobbo117/gpsTracker/assets/58577175/be021b8f-4aa4-4a6b-95e0-d9151d2ecfd4)


<br>

## Supported Protocols

The **gpsTracker** software, written in C++ using the Arduimo IDE, can transmit location data using MQTT

<br>

## Formatting Options

The **gpsTracker** software collects the GPS values 
for periodic transmission via the **Hologram** IoT cellular 
platform to the listed website services.

<br>

### Supported Services via Cellular

-    https://io.adafruit.com/ MQTT for dashboard and (potential) IFTTT webhook route


<br>


### Builtin Modules

-   An optional [`SSD1306` OLED display is useful for testing](https://www.amazon.com/Dorhea-Display-SSD1306-Self-Luminous-Raspberry/dp/B0837DLWVH/ref=sr_1_20?crid=2V7Q8UDD9PKSF&keywords=oled+128x64&qid=1686514637&s=electronics&sprefix=64x128+oled%2Celectronics%2C128&sr=1-20)

<br>


<!----------------------------------------------------------------------------->

[Badge License]: https://img.shields.io/badge/License-Unknown-808080.svg?style=for-the-badge

[License]: 5

