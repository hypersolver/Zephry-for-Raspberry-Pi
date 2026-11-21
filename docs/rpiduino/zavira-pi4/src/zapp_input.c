/* SPDX-License-Identifier: Apache-2.0 */
#include <lvgl.h>
#include <string.h>
#include <rpiduino/zapp.h>
#include <rpiduino/zapp_input.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>

struct pointer_event {
	uint16_t x;
	uint16_t y;
	bool pressed;
};

K_MSGQ_DEFINE(pointer_events, sizeof(struct pointer_event), 16, 4);
K_MSGQ_DEFINE(keys, sizeof(uint32_t), 8, 4);
static lv_group_t *group;
static lv_obj_t *cursor;
static struct pointer_event pointer = {320, 240, false};

static int coordinate(const char *digits)
{
	int value = 0;

	for (int i = 0; i < 3; i++) {
		if (digits[i] < '0' || digits[i] > '9') {
			return -1;
		}
		value = value * 10 + digits[i] - '0';
	}
	return value;
}

bool zapp_input_command(const char *command)
{
	if (strlen(command) == 15 && strncmp(command, "MOUSE ", 6) == 0 &&
	    command[9] == ' ' && command[13] == ' ' &&
	    (command[14] == '0' || command[14] == '1')) {
		int x = coordinate(command + 6);
		int y = coordinate(command + 10);

		if (x >= 0 && x < 640 && y >= 0 && y < 480) {
			struct pointer_event event = {x, y, command[14] == '1'};
			return k_msgq_put(&pointer_events, &event, K_NO_WAIT) == 0;
		}
	}

	static const struct {
		const char *command;
		uint32_t key;
	} mapping[] = {{"KEY NEXT", LV_KEY_NEXT}, {"KEY PREV", LV_KEY_PREV},
		       {"KEY ENTER", LV_KEY_ENTER}};
	for (size_t i = 0; i < ARRAY_SIZE(mapping); i++) {
		if (strcmp(command, mapping[i].command) == 0) {
			return k_msgq_put(&keys, &mapping[i].key, K_NO_WAIT) == 0;
		}
	}
	if (strcmp(command, "KEY ESC") == 0) {
		zapp_request(ZAPP_DESKTOP);
		return true;
	}
	return false;
}

static void pointer_read(lv_indev_t *indev, lv_indev_data_t *data)
{
	ARG_UNUSED(indev);
	if (k_msgq_get(&pointer_events, &pointer, K_NO_WAIT) == 0) {
		lv_obj_set_hidden(cursor, false);
	}
	data->point.x = pointer.x;
	data->point.y = pointer.y;
	data->state = pointer.pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
	data->continue_reading = k_msgq_num_used_get(&pointer_events) != 0;
}

static void keyboard_read(lv_indev_t *indev, lv_indev_data_t *data)
{
	ARG_UNUSED(indev);
	static uint32_t key;
	static bool pressed;

	if (pressed) {
		pressed = false;
	} else {
		pressed = k_msgq_get(&keys, &key, K_NO_WAIT) == 0;
	}
	data->key = key;
	data->state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
	data->continue_reading = pressed || k_msgq_num_used_get(&keys) != 0;
}

void zapp_input_add_button(void *object)
{
	if (group) {
		lv_group_add_obj(group, object);
	}
}

void zapp_input_init(void)
{
	group = lv_group_create();
	lv_indev_t *keypad = lv_indev_create();
	lv_indev_set_type(keypad, LV_INDEV_TYPE_KEYPAD);
	lv_indev_set_group(keypad, group);
	lv_indev_set_read_cb(keypad, keyboard_read);

	cursor = lv_obj_create(lv_layer_top());
	lv_obj_remove_style_all(cursor);
	lv_obj_set_size(cursor, 9, 9);
	lv_obj_remove_flag(cursor, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_style_bg_color(cursor, lv_color_hex(0xf5efff), 0);
	lv_obj_set_style_bg_opa(cursor, LV_OPA_COVER, 0);
	lv_obj_set_style_border_color(cursor, lv_color_hex(0x332167), 0);
	lv_obj_set_style_border_width(cursor, 1, 0);
	lv_obj_set_hidden(cursor, true);
	lv_indev_t *mouse = lv_indev_create();
	lv_indev_set_type(mouse, LV_INDEV_TYPE_POINTER);
	lv_indev_set_read_cb(mouse, pointer_read);
	lv_indev_set_cursor(mouse, cursor);
}
