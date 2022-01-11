# SmartGarden Technion IoT project

[SmartGarden](https://github.com/Exital/smartGarden) is an IoT gardening system consisting of endpoints which monitors soil moisture and temperature of the environment, and an RPI-based central server that displays the data and allows running irrigation and calibration commands to each of the endpoints.

[node-red project](https://github.com/Exital/SmartGardenPI_nodered) - This is the repo for the node-red project.

## Hardware

- [Raspberrypi 3 model b+](https://www.raspberrypi.com/products/raspberry-pi-3-model-b-plus/) -  used as a local node red server to control all units.
- [ESP32](https://www.espressif.com) - used as the end point controller.

### Sensors and motors
- [Capacitive soil moisture sensor](https://www.amazon.co.uk/Rfvtgb-Capacitive-Moisture-Corrosion-Resistant/dp/B094VGKNQK/ref=sr_1_2_sspa?crid=3CALEXSGL34YO&keywords=capacitive+soil+moisture+sensor&qid=1641928400&sprefix=capacitive+soil+mo%2Caps%2C131&sr=8-2-spons&psc=1&spLa=ZW5jcnlwdGVkUXVhbGlmaWVyPUEyWFBXS1VZOUxaN0VMJmVuY3J5cHRlZElkPUEwNzUwNzEyM0RINlQ4SEhISzVCMCZlbmNyeXB0ZWRBZElkPUEwMjgyNTcxM0hCV0xPWks4VjA5TSZ3aWRnZXROYW1lPXNwX2F0ZiZhY3Rpb249Y2xpY2tSZWRpcmVjdCZkb05vdExvZ0NsaWNrPXRydWU=) - used to monitor the soil moisture and the level of water in the irrigation tank.
- [BMP280](https://www.amazon.co.uk/Barometric-Temperature-calibrated-Barometer-Altimeter/dp/B07BD5L91Y/ref=sr_1_4?crid=2TPV9TOF3T9Q4&keywords=bmp280&qid=1641928489&sprefix=bmp280%2Caps%2C164&sr=8-4) - used to monitor temperature.
- [Mini servo SG90](https://www.amazon.co.uk/ULTECHNOVO-Micro-Helicopter-Airplane-Controls/dp/B08PVCT9Z4/ref=sr_1_4_sspa?crid=OXO0ZNZUXTLR&keywords=mini%2Bservo&qid=1641928547&sprefix=mini%2Bservo%2Caps%2C135&sr=8-4-spons&spLa=ZW5jcnlwdGVkUXVhbGlmaWVyPUEzN0JMTlpMSlhTNzhZJmVuY3J5cHRlZElkPUEwMTMyMzgyMVJBM01DOVJHRjRZSCZlbmNyeXB0ZWRBZElkPUEwNTkzODY1MjA2TVJHSlRWNFRMSSZ3aWRnZXROYW1lPXNwX2F0ZiZhY3Rpb249Y2xpY2tSZWRpcmVjdCZkb05vdExvZ0NsaWNrPXRydWU&th=1) - used as a valve for the irrigation system.
- [Photoresistor sensor](https://he.aliexpress.com/item/32701608104.html?spm=a2g0o.productlist.0.0.4df8270eD0duhN&algo_pvid=e78bfb57-1136-402b-b3fc-9357bea1112a&algo_exp_id=e78bfb57-1136-402b-b3fc-9357bea1112a-7&pdp_ext_f=%7B%22sku_id%22%3A%2260696331493%22%7D&pdp_pi=-1%3B0.68%3B-1%3B-1%40salePrice%3BUSD%3Bsearch-mainSearch) - used to monitor the environment's lighting situation.
- [Battery](https://he.aliexpress.com/item/1005003229199497.html?spm=a2g0o.productlist.0.0.50a353ddhxWx6U&algo_pvid=02276440-738b-4b65-a0ba-6420fbd68675&aem_p4p_detail=20220111111800788025246049380079564831&algo_exp_id=02276440-738b-4b65-a0ba-6420fbd68675-49&pdp_ext_f=%7B%22sku_id%22%3A%2212000024859221512%22%7D&pdp_pi=-1%3B7.56%3B-1%3BUSD+3.16%40salePrice%3BUSD%3Bsearch-mainSearch) - power source converted to 5v using a battery case.
- []


### Install dependencies

> Open the `arduino IDE` -> Tools -> Manage Libraries and install the following:
- WIFI
- Adafruit BMP280 Library
- PubSubClient
- ServoESP32
