# juice-sub-test
Juice subscriber example

Subscribes to a MQTT topic "juice" and displays information about charging/battery consumption published by the [Juice](https://github.com/icboredman/juice) app.

MQTT client code uses Eclipse C++ library [paho.mqtt.cpp](https://github.com/eclipse/paho.mqtt.cpp)

### Compiling
* `cd build`
* `cmake ..`
* `make`
