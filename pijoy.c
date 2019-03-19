/*
 * Sega megadrive gamepad driver for raspberry pi gpio
 *
 * Copyright (c) 2015 boolivar@gmail.com
 *
 * Based on the work of Vojtech Pavlik (db9 module)
 */

/*
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/time.h>

MODULE_AUTHOR("boolivar <boolivar@gmail.com>");
MODULE_DESCRIPTION("Sega megadrive gamepad driver for raspberry pi gpio");
MODULE_LICENSE("GPL");

struct pijoy_config {
    int args[2];
    unsigned int nargs;
};

#define GAMEPADS_MAX		2
static struct pijoy_config config[GAMEPADS_MAX] __initdata;

module_param_array_named(dev1, config[0].args, int, &config[0].nargs, 0);
MODULE_PARM_DESC(dev1, "First gamepad (<gpio#>,<type>)");
module_param_array_named(dev2, config[1].args, int, &config[1].nargs, 0);
MODULE_PARM_DESC(dev2, "Second gamepad (<gpio#>,<type>)");

#define ARG_GPIO		0
#define ARG_MODE		1

#define DB9_MULTI_STICK		0x01
#define DB9_MULTI2_STICK	0x02
#define DB9_GENESIS_PAD		0x03
#define DB9_GENESIS5_PAD	0x05
#define DB9_GENESIS6_PAD	0x06
#define DB9_SATURN_PAD		0x07
#define DB9_MULTI_0802		0x08
#define DB9_MULTI_0802_2	0x09
#define DB9_CD32_PAD		0x0A
#define DB9_SATURN_DPP		0x0B
#define DB9_SATURN_DPP_2	0x0C
#define DB9_MAX_PAD		0x0D

#define DB9_UP			0x01
#define DB9_DOWN		0x02
#define DB9_LEFT		0x04
#define DB9_RIGHT		0x08
#define DB9_FIRE1		0x10
#define DB9_FIRE2		0x20
#define DB9_FIRE3		0x40
#define DB9_FIRE4		0x80

#define LEVEL_SELECT		0
#define LEVEL_UNSELECT		1

#define GENESIS6_DELAY	14
#define REFRESH_TIME	HZ/100

struct db9_mode_data {
    const char *name;
    const short *buttons;
    int n_buttons;
    int n_pads;
    int n_axis;
    int bidirectional;
    int reverse;
};

struct port {
    struct gpio* js;
    int size;
};

struct gamepad {
    char phys[32];
    struct input_dev *dev;

    int port;
    int mode;
    int used;
};

static const short db9_multi_btn[] = { BTN_TRIGGER, BTN_THUMB };
static const short db9_genesis_btn[] = { BTN_START, BTN_A, BTN_B, BTN_C, BTN_X, BTN_Y, BTN_Z, BTN_MODE };
static const short db9_cd32_btn[] = { BTN_A, BTN_B, BTN_C, BTN_X, BTN_Y, BTN_Z, BTN_TL, BTN_TR, BTN_START };
static const short db9_abs[] = { ABS_X, ABS_Y, ABS_RX, ABS_RY, ABS_RZ, ABS_Z, ABS_HAT0X, ABS_HAT0Y, ABS_HAT1X, ABS_HAT1Y };

static const struct db9_mode_data db9_modes[] = {
    { NULL,					 NULL,		  0,  0,  0,  0,  0 },
    { "Multisystem joystick",		 db9_multi_btn,	  1,  1,  2,  1,  1 },
    { "Multisystem joystick (2 fire)",	 db9_multi_btn,	  2,  1,  2,  1,  1 },
    { "Genesis pad",			 db9_genesis_btn, 4,  1,  2,  1,  1 },
    { NULL,					 NULL,		  0,  0,  0,  0,  0 },
    { "Genesis 5 pad",			 db9_genesis_btn, 6,  1,  2,  1,  1 },
    { "Genesis 6 pad",			 db9_genesis_btn, 8,  1,  2,  1,  1 },
    { "Saturn pad",				 db9_cd32_btn,	  9,  6,  7,  0,  1 },
    { "Multisystem (0.8.0.2) joystick",	 db9_multi_btn,	  1,  1,  2,  1,  1 },
    { "Multisystem (0.8.0.2-dual) joystick", db9_multi_btn,	  1,  2,  2,  1,  1 },
    { "Amiga CD-32 pad",			 db9_cd32_btn,	  7,  1,  2,  1,  1 },
    { "Saturn dpp",				 db9_cd32_btn,	  9,  6,  7,  0,  0 },
    { "Saturn dpp dual",			 db9_cd32_btn,	  9,  12, 7,  0,  0 },
};

#define gpio_in(pin, name) { \
    .gpio   = pin, \
    .flags  = GPIOF_IN, \
    .label  = name, \
}

#define gpio_out(pin, name) { \
    .gpio   = pin, \
    .flags  = GPIOF_OUT_INIT_HIGH, \
    .label  = name, \
}

static const struct gpio js0[] = {
    gpio_in(2, "js0d0"),
    gpio_in(3, "js0d1"),
    gpio_in(4, "js0d2"),
    gpio_in(17, "js0d3"),
    gpio_in(27, "js0d4"),
    gpio_in(22, "js0d5"),
    gpio_out(23, "js0sel"),
};

static const struct gpio js1[] = {
    gpio_in(10, "js1d0"),
    gpio_in(9, "js1d1"),
    gpio_in(11, "js1d2"),
    gpio_in(0, "js1d3"),
    gpio_in(5, "js1d4"),
    gpio_in(6, "js1d5"),
    gpio_out(12, "js1sel"),
};

static const struct gpio *js[] = { js0, js1 };

static struct {
    int used;
    struct mutex mutx;
    struct timer_list timer;
    struct gamepad *gamepads[GAMEPADS_MAX];
} driver = {0};

static void write_controls(int level);
static void write_control(const struct gpio* port, int level);
static int read_data(const struct gpio* port);

static void dev_poll(unsigned long private)
{
    struct input_dev *dev;
    int i, data;

    write_controls(LEVEL_UNSELECT); /* 1 */
    udelay(GENESIS6_DELAY);

    for (i = 0; i < GAMEPADS_MAX; ++i) {
        if (driver.gamepads[i] && driver.gamepads[i]->used) {
            dev = driver.gamepads[i]->dev;

            data = read_data(js[driver.gamepads[i]->port]);
            input_report_abs(dev, ABS_X, (data & DB9_RIGHT ? 0 : 1) - (data & DB9_LEFT ? 0 : 1));
            input_report_abs(dev, ABS_Y, (data & DB9_DOWN  ? 0 : 1) - (data & DB9_UP   ? 0 : 1));
            input_report_key(dev, BTN_B, ~data & DB9_FIRE1);
            input_report_key(dev, BTN_C, ~data & DB9_FIRE2);
        }
    }

    write_controls(LEVEL_SELECT);
    udelay(GENESIS6_DELAY);

    for (i = 0; i < GAMEPADS_MAX; ++i) {
        if (driver.gamepads[i] && driver.gamepads[i]->used) {
            dev = driver.gamepads[i]->dev;

            data = read_data(js[driver.gamepads[i]->port]);
            input_report_key(dev, BTN_A, ~data & DB9_FIRE1);
            input_report_key(dev, BTN_START, ~data & DB9_FIRE2);
        }
    }

    write_controls(LEVEL_UNSELECT); /* 2 */
    udelay(GENESIS6_DELAY);

    write_controls(LEVEL_SELECT);
    udelay(GENESIS6_DELAY);

    write_controls(LEVEL_UNSELECT); /* 3 */
    udelay(GENESIS6_DELAY);

    for (i = 0; i < GAMEPADS_MAX; ++i) {
        if (driver.gamepads[i] && driver.gamepads[i]->used) {
            dev = driver.gamepads[i]->dev;

            data = read_data(js[driver.gamepads[i]->port]);
            input_report_key(dev, BTN_X,    ~data & DB9_LEFT);
            input_report_key(dev, BTN_Y,    ~data & DB9_DOWN);
            input_report_key(dev, BTN_Z,    ~data & DB9_UP);
            input_report_key(dev, BTN_MODE, ~data & DB9_RIGHT);
        }
    }

    write_controls(LEVEL_SELECT);
    udelay(GENESIS6_DELAY);

    write_controls(LEVEL_UNSELECT); /* 4 */
    udelay(GENESIS6_DELAY);

    write_controls(LEVEL_SELECT);

    for (i = 0; i < GAMEPADS_MAX; ++i) {
        if (driver.gamepads[i] && driver.gamepads[i]->used) {
            input_sync(driver.gamepads[i]->dev);
        }
    }

    mod_timer(&driver.timer, jiffies + REFRESH_TIME);
}

static void write_controls(int level) {
    int i;
    for (i = 0; i < GAMEPADS_MAX; ++i) {
        if (driver.gamepads[i] && driver.gamepads[i]->used) {
            write_control(js[driver.gamepads[i]->port], level);
        }
    }
}

static void write_control(const struct gpio* port, int level) {
    gpio_set_value(port[ARRAY_SIZE(js0) - 1].gpio, level);
}

static int read_data(const struct gpio* port) {
    int i;
    int value = 0;
    int mask = 1;

    for (i = 0; i < ARRAY_SIZE(js0); ++i) {
        int pin = gpio_get_value(port[i].gpio);
        if (pin) {
            value |= mask;
        }
        mask <<= 1;
    }

    return value;
}

static int dev_open(struct input_dev *dev)
{
    struct gamepad *pad = input_get_drvdata(dev);
    int err;

    err = mutex_lock_interruptible(&driver.mutx);
    if (err) {
        return err;
    }

    err = gpio_request_array(js[pad->port], ARRAY_SIZE(js0));
    if (err == 0 && driver.used == 0) {
        mod_timer(&driver.timer, jiffies + REFRESH_TIME);
        printk(KERN_DEBUG "pijoy.c: request gpio ok, size=%d\n", ARRAY_SIZE(js0));
    }

    if (err == 0) {
        ++driver.used;
        ++pad->used;
    }

    mutex_unlock(&driver.mutx);

    printk(KERN_DEBUG "pijoy.c: open device %d(%d used), err=%d\n", pad->port, pad->used, err);
    return err;
}

static void dev_close(struct input_dev *dev)
{
    struct gamepad *pad = input_get_drvdata(dev);

    mutex_lock(&driver.mutx);
    if (!--driver.used) {
        del_timer_sync(&driver.timer);
        gpio_free_array(js[pad->port], ARRAY_SIZE(js0));
        printk(KERN_DEBUG "pijoy.c: free gpio, size=%d\n", ARRAY_SIZE(js0));
    }
    --pad->used;
    mutex_unlock(&driver.mutx);
    printk(KERN_DEBUG "pijoy.c: close device %d(%d used)\n", pad->port, pad->used);
}

static struct gamepad __init *db9_probe(int index, int mode)
{
    struct gamepad *pad;
    const struct db9_mode_data *pad_mode;
    struct input_dev *input_dev;
    int i;
    int err;

    if (mode < 1 || mode >= DB9_MAX_PAD || !db9_modes[mode].n_buttons) {
        printk(KERN_ERR "pijoy.c: Bad device type %d\n", mode);
        err = -EINVAL;
        goto err_out;
    }
    pad_mode = &db9_modes[mode];

    pad = kzalloc(sizeof(struct gamepad), GFP_KERNEL);
    if (!pad) {
        printk(KERN_ERR "pijoy.c: Not enough memory\n");
        err = -ENOMEM;
        goto err_out;
    }

    pad->port = index;
    pad->mode = mode;

    pad->dev = input_dev = input_allocate_device();
    if (!input_dev) {
        printk(KERN_ERR "pijoy.c: Not enough memory for input device\n");
        err = -ENOMEM;
        goto err_unalloc;
    }

    snprintf(pad->phys, sizeof(pad->phys), "%s/input%d", "pijoy", index);

    input_dev->name = pad_mode->name;
    input_dev->phys = pad->phys;
    input_dev->id.bustype = BUS_HOST;
    input_dev->id.vendor = 0x0002;
    input_dev->id.product = mode;
    input_dev->id.version = 0x0100;

    input_set_drvdata(input_dev, pad);

    input_dev->open = dev_open;
    input_dev->close = dev_close;

    input_dev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
    for (i = 0; i < pad_mode->n_buttons; i++) {
        set_bit(pad_mode->buttons[i], input_dev->keybit);
    }
    for (i = 0; i < pad_mode->n_axis; i++) {
        if (i < 2) {
            input_set_abs_params(input_dev, db9_abs[i], -1, 1, 0, 0);
        } else {
            input_set_abs_params(input_dev, db9_abs[i], 1, 255, 0, 0);
        }
    }

    err = input_register_device(input_dev);
    if (err)
        goto err_free_dev;

    printk(KERN_NOTICE "pijoy.c: device %d registered\n", index);
    return pad;

err_free_dev:
    input_free_device(input_dev);
err_unalloc:
    kfree(pad);
err_out:
    return ERR_PTR(err);
}

static void db9_remove(struct gamepad *pad)
{
    input_unregister_device(pad->dev);
    kfree(pad);
    printk(KERN_NOTICE "pijoy.c: unregister device %d\n", pad->port);
}

static int __init pijoy_init(void)
{
    int i;
    int devices = 0;
    int err = 0;

    mutex_init(&driver.mutx);
    init_timer(&driver.timer);
    driver.timer.function = dev_poll;

    for (i = 0; i < GAMEPADS_MAX; ++i) {
        if (config[i].nargs > 0) {
            if (config[i].nargs < 2) {
                printk(KERN_ERR "pijoy.c: Device type must be specified.\n");
                err = -EINVAL;
                break;
            }

            driver.gamepads[i] = db9_probe(config[i].args[ARG_GPIO], config[i].args[ARG_MODE]);
            if (IS_ERR(driver.gamepads[i])) {
                err = PTR_ERR(driver.gamepads[i]);
                break;
            }

            ++devices;
        }
    }

    if (err) {
        printk(KERN_ERR "pijoy.c: error on init: %d.\n", err);
        while (--i >= 0) {
            if (driver.gamepads[i]) {
                db9_remove(driver.gamepads[i]);
            }
        }
        return err;
    }

    printk(KERN_NOTICE "pijoy.c: we have %d devices.\n", devices);

    return (devices > 0) ? 0 : -ENODEV;
}

static void __exit pijoy_exit(void)
{
    int i;

    for (i = 0; i < GAMEPADS_MAX; ++i) {
        if (driver.gamepads[i]) {
            db9_remove(driver.gamepads[i]);
        }
    }
}

module_init(pijoy_init);
module_exit(pijoy_exit);
