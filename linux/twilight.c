/*
 * i2c Driver for TWILight
 *
 * Copyright (C) Josef Gajdusek <atx@atx.name>
 *
 * GitHub project:
 * https://github.com/atalax/TWILight
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * */

#include <linux/printk.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/err.h>
#include <linux/leds.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>


#define TWILIGHT_GPIO_BASE	0x00
#define TWILIGHT_LED0	0x10
#define TWILIGHT_LED1	0x11
#define TWILIGHT_LED2	0x12
#define TWILIGHT_LED3	0x13
#define TWILIGHT_MAGIC	0x14
#define TWILIGHT_FW		0x17

#define TWILIGHT_GPIO_COUNT		10
#define TWILIGHT_GPIO_ADDR(off) (TWILIGHT_GPIO_BASE + (off))

struct twilight_led {
	struct twilight_data *data;
	struct led_classdev cdev;
	struct work_struct work;
	u8 addr;
	char name[20];
};

struct twilight_data {
	struct regmap *regmap;
	struct mutex mutex;
	struct gpio_chip gpio;
	struct twilight_led leds[4];
};

static void twilight_gpio_set(struct gpio_chip *gpio, unsigned off, int value)
{
	struct twilight_data *data =
			container_of(gpio, struct twilight_data, gpio);
	regmap_write(data->regmap, TWILIGHT_GPIO_ADDR(off), (!!value << 1) | 1);
}

static int twilight_gpio_get(struct gpio_chip *gpio, unsigned off)
{
	struct twilight_data *data =
			container_of(gpio, struct twilight_data, gpio);
	int val;

	regmap_read(data->regmap, TWILIGHT_GPIO_ADDR(off), &val);
	return !!(val & 0x4);
}

static int twilight_gpio_set_dir(struct gpio_chip *gpio, unsigned off, bool to)
{
	struct twilight_data *data =
			container_of(gpio, struct twilight_data, gpio);
	return regmap_write(data->regmap, TWILIGHT_GPIO_ADDR(off), !!to);
}

static int twilight_gpio_output(struct gpio_chip *gpio, unsigned off, int value)
{
	return twilight_gpio_set_dir(gpio, off, true);
}

static int twilight_gpio_input(struct gpio_chip *gpio, unsigned off)
{
	return twilight_gpio_set_dir(gpio, off, false);
}

static void twilight_brightness_set(struct led_classdev *cdev, enum led_brightness bri)
{
	struct twilight_led *led =
			container_of(cdev, struct twilight_led, cdev);
	schedule_work(&led->work);
}

static void twilight_work(struct work_struct *work)
{
	struct twilight_led *led =
			container_of(work, struct twilight_led, work);
	regmap_write(led->data->regmap, led->addr, led->cdev.brightness);
}

static struct regmap_access_table twilight_regmap_read = {
		.yes_ranges = &(struct regmap_range)
			regmap_reg_range(0x00, TWILIGHT_FW),
		.n_yes_ranges = 1,
};

static struct regmap_access_table twilight_regmap_write = {
		.yes_ranges = &(struct regmap_range)
			regmap_reg_range(0x00, TWILIGHT_LED3),
		.n_yes_ranges = 1,
};

static struct regmap_access_table twilight_regmap_volatile = {
		.yes_ranges = &(struct regmap_range)
			regmap_reg_range(TWILIGHT_GPIO_BASE, TWILIGHT_GPIO_BASE + TWILIGHT_GPIO_COUNT),
		.n_yes_ranges = 1,
};

static struct regmap_config twilight_regmap_config = {
		.reg_bits = 8,
		.val_bits = 8,

		.rd_table = &twilight_regmap_read,
		.wr_table = &twilight_regmap_write,
		.volatile_table = &twilight_regmap_volatile,

		.cache_type = REGCACHE_FLAT,
};

static int twilight_probe(struct i2c_client *cli, const struct i2c_device_id *id)
{
	struct twilight_data *data;
	int i;
	int ret;

	data = devm_kzalloc(&cli->dev, sizeof(struct twilight_data), GFP_KERNEL);

	if (data == NULL)
		return -ENOMEM;

	mutex_init(&data->mutex);
	i2c_set_clientdata(cli, data);
	data->regmap = devm_regmap_init_i2c(cli, &twilight_regmap_config);

	/* Init the PWM controlled LEDs */
	data->leds[0].addr = TWILIGHT_LED0;
	data->leds[1].addr = TWILIGHT_LED1;
	data->leds[2].addr = TWILIGHT_LED2;
	data->leds[3].addr = TWILIGHT_LED3;

	/* Generating LED names dynamically allows more TWILights on single system */
	snprintf(data->leds[0].name, ARRAY_SIZE(data->leds[0].name),
				"twilight-%d:%s:", cli->adapter->nr, "orange");
	snprintf(data->leds[1].name, ARRAY_SIZE(data->leds[0].name),
				"twilight-%d:%s:", cli->adapter->nr, "blue");
	snprintf(data->leds[2].name, ARRAY_SIZE(data->leds[0].name),
				"twilight-%d:%s:", cli->adapter->nr, "green");
	snprintf(data->leds[3].name, ARRAY_SIZE(data->leds[0].name),
				"twilight-%d:%s:", cli->adapter->nr, "red");

	for (i = 0; i < ARRAY_SIZE(data->leds); i++) { /* Populate with common values and register */
		data->leds[i].data = data;
		data->leds[i].cdev.brightness = LED_OFF;
		data->leds[i].cdev.max_brightness = 255;
		data->leds[i].cdev.brightness_set = twilight_brightness_set;
		data->leds[i].cdev.name = data->leds[i].name;
		INIT_WORK(&data->leds[i].work, twilight_work);

		ret = led_classdev_register(&cli->dev, &data->leds[i].cdev);
		if (ret) {
			dev_err(&cli->dev, "Failed to register LEDs!\n");
			i--;
			goto eled;
		}
	}

	/* Init GPIO */
	data->gpio.dev = &cli->dev;
	data->gpio.base = -1;
	data->gpio.owner = THIS_MODULE;
	data->gpio.ngpio = 10;
	data->gpio.can_sleep = true;
	data->gpio.set = twilight_gpio_set;
	data->gpio.get = twilight_gpio_get;
	data->gpio.direction_output = twilight_gpio_output;
	data->gpio.direction_input = twilight_gpio_input;

	ret = gpiochip_add(&data->gpio);
	if (ret) {
		dev_err(&cli->dev, "Failed to register GPIO!\n");
		goto eled;
	}

	return 0;

eled:
	for (; i >= 0; i--)
		led_classdev_unregister(&data->leds[i].cdev);

	return ret;
}

static int twilight_remove(struct i2c_client *cli)
{
	struct twilight_data *data = i2c_get_clientdata(cli);
	int i;
	int ret;

	for (i = 0; i < ARRAY_SIZE(data->leds); i++) {
		led_classdev_unregister(&data->leds[i].cdev);
		flush_work(&data->leds[i].work);
	}

	ret = gpiochip_remove(&data->gpio);
	if (ret)
		dev_err(&cli->dev, "Failed to unregister gpio %d\n", data->gpio.base);

	return ret;
}

static const struct i2c_device_id twilight_id[] = {
		{ "twilight", 0 },
		{ }
};

static struct i2c_driver twilight_driver = {
		.driver = {
				.name = "twilight",
				.owner = THIS_MODULE,
		},
		.probe = twilight_probe,
		.id_table = twilight_id,
		.remove = twilight_remove,
};

module_i2c_driver(twilight_driver);

MODULE_DESCRIPTION("TWILight i2c driver");
MODULE_AUTHOR("Josef Gajdusek <atx@atx.name>");
MODULE_LICENSE("GPL");
