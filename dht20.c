	// SPDX-License-Identifier: GPL-2.0-only

/*
 * Copyright (c) DumberThanUsual
 *
 * ASAIR DHT20 Humidity and Temperature Module *
 * 
 * (7-bit I2C slave address 0x40)
 * 
 */

#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/iio/iio.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/types.h>

#define DHT20_REG_STATUS 0x71
#define DHT20_REG_STATUS_MASK_BUSY (1u << 7)
#define DHT20_REG_STATUS_OK 0x18	

struct dht20_data {
	struct i2c_client *client;
	struct mutex lock; /* Lock to prevent concurrent reads of temperature readings */
    
	s64				timestamp;
	int				temperature;
	int				humidity;
};

static const struct iio_chan_spec dht20_channels[] = {
	{
		.type = IIO_TEMP,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),
	},
	{
		.type = IIO_HUMIDITYRELATIVE,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),
	},
};

/**
 * dht20_request() - Request a reading
 * @data: Struct comprising member elements of the device
 *
 * Requests a reading from the device and waits until the conversion is ready.
 */
static int dht20_request(struct dht20_data *data, char *buf)
{
	/*
	 * Sensor can take up to 100 ms to respond so execute a total of
	 * 10 retries to give the device sufficient time.
	 */
	int retries = 10;
	int ret;
	char trigger_cmd[3] = {0xAC, 0x33, 0x00};

	ret = (i2c_smbus_read_byte_data(data->client, DHT20_REG_STATUS) & DHT20_REG_STATUS_OK) != DHT20_REG_STATUS_OK;
	if (ret) {
		dev_err(&data->client->dev, "Status register abnormal\n");
		return ret;
    }

	ret = i2c_master_send(data->client, trigger_cmd, 3);
	if (ret < 0) {
		dev_err(&data->client->dev, "Trigger Error\n");
		return ret;
	}

	msleep(50);

	while (retries--) {
		ret = i2c_master_recv(data->client, buf, 7);
		if (ret < 0)
			return ret;

        if ((buf[0] & BIT(7)) == 0){
            data->timestamp = ktime_get_boottime_ns();
			return 0;
        }
        
		msleep(5);
	}
	dev_err(&data->client->dev, "Temperature conversion failed\n");

	return -ETIMEDOUT;
} 

static int dht20_update(struct dht20_data *data)
{   
	long tmp;
	int ret;
    char buf[8];      

	mutex_lock(&data->lock);

	ret = dht20_request(data, buf);
	if (ret)
		goto unlock;

    dev_err(&data->client->dev, buf);

	tmp = buf[5] >> 4 | buf[4] << 4 | (buf[3] & 0b00001111) << 12;
	tmp = ((tmp * 20000) - (5000 << 16)) >> 16;
	data->temperature = tmp;	

	tmp = buf[2]| buf[1] << 8;
	tmp = (tmp * 10000) >> 16;
	data->humidity = tmp;

unlock:
	mutex_unlock(&data->lock);
	return ret;
}

static int dht20_read(struct iio_dev *indio_dev,
			 struct iio_chan_spec const *chan,
			 int *val, int *val2, long mask)
{
	struct dht20_data *data = iio_priv(indio_dev);
	int ret;

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		ret = dht20_update(data);
		if (ret < 0)
			return ret;

        if (chan->type == IIO_TEMP)
		    *val = data->temperature;
	    else if (chan->type == IIO_HUMIDITYRELATIVE)
            *val = data->humidity;

		return IIO_VAL_INT;
	default:
		return -EINVAL;
	}
}

static const struct iio_info dht20_info = {
	.read_raw = dht20_read,
};

static int dht20_probe(struct i2c_client *i2c, const struct i2c_device_id *id)
{
	struct device *dev = &i2c->dev;
	struct dht20_data *data;
	struct iio_dev *indio_dev;
	int ret;


		dev_err(dev, "PROBE\n");

	indio_dev = devm_iio_device_alloc(dev, sizeof(*data));
	if (!indio_dev)
		return -ENOMEM;

	data = iio_priv(indio_dev);
	data->client = i2c;
	mutex_init(&data->lock);

	indio_dev->name = "dht20";
	indio_dev->channels = dht20_channels;
	indio_dev->num_channels = ARRAY_SIZE(dht20_channels);
	indio_dev->info = &dht20_info;
	indio_dev->modes = INDIO_DIRECT_MODE;

    ret = (i2c_smbus_read_byte_data(data->client, DHT20_REG_STATUS) & DHT20_REG_STATUS_OK) != DHT20_REG_STATUS_OK;
	if (ret) {
		dev_err(dev, "Status register abnormal\n");
		return ret;
	}

	msleep(100);

	ret = devm_iio_device_register(dev, indio_dev);
	if (ret) {
		dev_err(dev, "Failed to register IIO device\n");
		return ret;
	}

	return 0;
}

static const struct i2c_device_id dht20_id_table[] = {
	{ "dht20", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, dht20_id_table);

static const struct of_device_id dht20_of_match[] = {
	{ .compatible = "asair,dht20" },
	{ }
};
MODULE_DEVICE_TABLE(of, dht20_of_match);

static struct i2c_driver dht20_driver = {
	.driver = {
		.name = "dht20",
		.of_match_table = dht20_of_match,
	},
	.probe = dht20_probe,
	.id_table = dht20_id_table,
};
module_i2c_driver(dht20_driver);

MODULE_AUTHOR("DumberThanUsual");
MODULE_DESCRIPTION("ASAIR DHT20 Humidity and Temperature Sensor Driver");
MODULE_LICENSE("GPL");