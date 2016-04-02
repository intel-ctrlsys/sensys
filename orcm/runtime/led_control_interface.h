/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef LED_CONTROL_INTERFACE_H
#define LED_CONTROL_INTERFACE_H

#include <stdlib.h>

extern void init_led_control(char*, char*, char*);

extern int get_chassis_id_state(void);

extern int disable_chassis_id(void);

extern int enable_chassis_id(void);

extern int enable_chassis_id_with_timeout(int);

extern void fini_led_control(void);

#endif
