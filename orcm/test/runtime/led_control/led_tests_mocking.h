/*
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef CONTROL_LED_TESTS_MOCKING_H
#define CONTROL_LED_TESTS_MOCKING_H

extern int ipmi_open_session_count;
extern bool is_remote_node;
extern bool is_supported;

#ifdef __cplusplus
extern "C"{
#endif

    extern int __real_ipmi_cmd(unsigned short, unsigned char*, int,
        unsigned char*, int*, unsigned char*, char);

    extern int __real_ipmi_close(void);

    extern int __real_set_lan_options(char*, char*, char*, int, int, int, void*, int);

#ifdef __cplusplus
}
#endif

#endif // CONTROL_LED_TESTS_MOCKING_H
