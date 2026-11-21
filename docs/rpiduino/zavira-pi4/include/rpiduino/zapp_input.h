/* SPDX-License-Identifier: Apache-2.0 */
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool zapp_input_command(const char *command);
void zapp_input_init(void);
void zapp_input_add_button(void *object);

#ifdef __cplusplus
}
#endif
