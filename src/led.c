// LED status updates
//
// Copyright (C) 2021 Eric Callahan <arksine.code@gmail.com>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include "autoconf.h" // CONFIG_ENABLE_LED
#include "board/gpio.h" // gpio_out_setup
#include "board/misc.h" // timer_read_time
#include "ctr.h" // DECL_CTR
#include "sched.h" // DECL_INIT

DECL_CTR("DECL_LED_PIN " __stringify(CONFIG_STATUS_LED_PIN));
extern uint32_t led_gpio, led_gpio_high;

static struct gpio_out led;
static uint32_t last_time;
static uint32_t mode_end;
static uint8_t pwm_cnt;
static uint8_t bright;

void
led_init(void)
{
    led = gpio_out_setup(led_gpio, led_gpio_high);
    last_time = timer_read_time();
}
DECL_INIT(led_init);

void
led_blink_task(void)
{
    uint32_t now = timer_read_time();

    if (mode_end && timer_is_before(mode_end, now))
        mode_end = 0;

    if (mode_end) {
        if (timer_is_before(last_time + timer_from_us(166667), now)) {
            gpio_out_toggle(led);
            last_time = now;
        }
    } else {
        if (timer_is_before(last_time + timer_from_us(80000), now)) {
            last_time = now;
            bright = (bright + 1) & 31;
        }
        uint8_t b = (bright & 16) ? (31 - bright) : bright;
        gpio_out_write(led, ((++pwm_cnt & 15) < b) ? led_gpio_high : !led_gpio_high);
    }
}
DECL_TASK(led_blink_task);

void
led_identify(void)
{
    mode_end = timer_read_time() + timer_from_us(10000000);
}
