# Linux Device Driver for DHT20 Temperature and Humidity Sensor

## Usage
1. Place source file in kernel source at drivers/iio/temperature/
2. Add Kconfig option
```
config DHT20
	tristate "DHT20 temperature/humidity sensor"
	depends on I2C
	help
	  If you say yes here you get support for the ASAIR
	  DHT20 temperature/humidity sensor.

	  This driver can also be built as a module. If so, the module will
	  be called dht20.
```
3. Add Makefile config
```
obj-$(CONFIG_DHT20) += dht20.o
```
4. Configure Deviec Tree to include sensor description under the correct I2C device
```
dht20@38 {
	compatible = "asair,dht20";
	reg = <0x38>;
};	
```
5. Sensor will appear as an IIO device
