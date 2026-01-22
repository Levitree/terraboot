#ifndef __LED_H
#define __LED_H

#include "autoconf.h"

void led_identify(void);
#define led_is_enabled() CONFIG_ENABLE_LED

#endif // led.h
