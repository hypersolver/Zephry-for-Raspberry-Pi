/* SPDX-License-Identifier: Apache-2.0 */
#include <errno.h>
#include <lvgl.h>
#include <string.h>
#include <rpiduino/zapp.h>
#include <rpiduino/zapp_input.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/kernel.h>
#include <zephyr/llext/buf_loader.h>
#include <zephyr/llext/llext.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/crc.h>
#include <zephyr/sys/device_mmio.h>
#include <zephyr/sys/printk.h>
#include <zephyr/version.h>

#define BUNDLE_ADDRESS (IS_ENABLED(CONFIG_BOARD_RPI_4B) ? 0x02000000U : 0x47000000U)
#define BUNDLE_CAPACITY (128U * 1024U)

struct zapp {
	const char *name;
	const uint8_t *data;
	size_t size;
	struct llext *extension;
	void (*render)(const struct zapp_ui *, enum zapp_page);
	void (*tick)(uint32_t);
};

static struct zapp desktop = {.name = "desktop"};
static struct zapp demo = {.name = "lvgl-demo"};
static struct zapp benchmark = {.name = "lvgl-benchmark"};
static atomic_t pending_page;
static uint32_t redraws;

void zapp_request(enum zapp_page page)
{
	if (page >= ZAPP_DESKTOP && page <= ZAPP_BENCHMARK) {
		atomic_set(&pending_page, page);
	}
}

static void scene(uint32_t color)
{
	lv_obj_t *screen = lv_screen_active();

	lv_obj_clean(screen);
	lv_obj_set_style_bg_color(screen, lv_color_hex(color), 0);
	lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
}

static void panel(int x, int y, int width, int height, uint32_t color)
{
	lv_obj_t *object = lv_obj_create(lv_screen_active());

	lv_obj_remove_style_all(object);
	lv_obj_remove_flag(object, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_pos(object, x, y);
	lv_obj_set_size(object, width, height);
	lv_obj_set_style_bg_color(object, lv_color_hex(color), 0);
	lv_obj_set_style_bg_opa(object, LV_OPA_COVER, 0);
}

static void *label(int x, int y, const char *text, uint32_t color, int large)
{
	lv_obj_t *object = lv_label_create(lv_screen_active());

	lv_obj_set_pos(object, x, y);
	lv_obj_set_style_text_color(object, lv_color_hex(color), 0);
	if (large) {
		lv_obj_set_style_text_font(object, &lv_font_montserrat_28, 0);
	}
	lv_label_set_text(object, text);
	return object;
}

static void set_text(void *object, const char *text)
{
	lv_label_set_text(object, text);
}

static void open_button(lv_event_t *event)
{
	zapp_request((enum zapp_page)(uintptr_t)lv_event_get_user_data(event));
}

static void button(int x, int y, int width, int height, const char *text,
		   uint32_t color, enum zapp_page destination)
{
	lv_obj_t *object = lv_button_create(lv_screen_active());
	lv_obj_t *caption = lv_label_create(object);

	lv_obj_set_pos(object, x, y);
	lv_obj_set_size(object, width, height);
	lv_obj_set_style_radius(object, 0, 0);
	lv_obj_set_style_bg_color(object, lv_color_hex(color), 0);
	lv_obj_set_style_text_color(caption, lv_color_hex(0xffffff), 0);
	lv_label_set_text(caption, text);
	lv_obj_center(caption);
	lv_obj_add_event_cb(object, open_button, LV_EVENT_CLICKED, (void *)(uintptr_t)destination);
	zapp_input_add_button(object);
}

static void *bar(int x, int y, int width, int height, uint32_t color)
{
	lv_obj_t *object = lv_bar_create(lv_screen_active());

	lv_obj_set_pos(object, x, y);
	lv_obj_set_size(object, width, height);
	lv_obj_set_style_bg_color(object, lv_color_hex(color), LV_PART_INDICATOR);
	return object;
}

static void set_bar(void *object, unsigned int percent)
{
	lv_bar_set_value(object, (int32_t)percent, LV_ANIM_OFF);
}

struct triangle {
	lv_color_t color;
	struct zapp_point points[3];
};

static void draw_triangle(lv_event_t *event)
{
	struct triangle *shape = lv_event_get_user_data(event);
	lv_draw_triangle_dsc_t draw;
	lv_area_t bounds;

	lv_obj_get_coords(lv_event_get_target_obj(event), &bounds);
	lv_draw_triangle_dsc_init(&draw);
	draw.color = shape->color;
	for (size_t i = 0; i < ARRAY_SIZE(shape->points); i++) {
		draw.p[i].x = bounds.x1 + shape->points[i].x;
		draw.p[i].y = bounds.y1 + shape->points[i].y;
	}
	lv_draw_triangle(lv_event_get_layer(event), &draw);
}

static void free_triangle(lv_event_t *event)
{
	lv_free(lv_event_get_user_data(event));
}

static void *triangle(int x, int y, int size, uint32_t color)
{
	struct triangle *shape = lv_malloc_zeroed(sizeof(*shape));
	if (!shape) {
		return NULL;
	}

	lv_obj_t *object = lv_obj_create(lv_screen_active());
	lv_obj_remove_style_all(object);
	lv_obj_remove_flag(object, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
	lv_obj_set_pos(object, x, y);
	lv_obj_set_size(object, size, size);
	shape->color = lv_color_hex(color);
	lv_obj_add_event_cb(object, draw_triangle, LV_EVENT_DRAW_MAIN, shape);
	lv_obj_add_event_cb(object, free_triangle, LV_EVENT_DELETE, shape);
	lv_obj_set_user_data(object, shape);
	return object;
}

static void set_triangle(void *object, const struct zapp_point points[3])
{
	if (object) {
		struct triangle *shape = lv_obj_get_user_data(object);
		memcpy(shape->points, points, sizeof(shape->points));
		lv_obj_invalidate(object);
	}
}

static void redraw_complete(lv_event_t *event)
{
	ARG_UNUSED(event);
	redraws++;
}

static uint32_t redraw_count(void)
{
	return redraws;
}

static void report_benchmark(uint32_t count, uint32_t elapsed_ms)
{
	printk("RPiduino BENCH: redraws=%u elapsed_ms=%u\n", count, elapsed_ms);
}

static const struct zapp_ui ui = {
	.version = KERNEL_VERSION_STRING, .scene = scene, .panel = panel, .label = label,
	.set_text = set_text, .button = button, .bar = bar, .set_bar = set_bar,
	.triangle = triangle, .set_triangle = set_triangle,
	.redraw_count = redraw_count, .report_benchmark = report_benchmark,
};

static int locate_zapps(void)
{
	mm_reg_t mapped;

	device_map(&mapped, BUNDLE_ADDRESS, BUNDLE_CAPACITY, K_MEM_CACHE_WB);
	const uint8_t *bundle = (const uint8_t *)(uintptr_t)mapped;
	uint32_t version = sys_get_le32(bundle + 4);
	if (memcmp(bundle, "ZAPP", 4) != 0 || version < 1 || version > 3) {
		return -ENOENT;
	}

	struct zapp *slots[] = {&desktop, &demo, &benchmark};
	size_t count = version == 3 ? ARRAY_SIZE(slots) : 2;
	size_t offset = 8 + count * 8;
	for (size_t i = 0; i < count; i++) {
		size_t size = sys_get_le32(bundle + 8 + i * 8);
		uint32_t expected = sys_get_le32(bundle + 12 + i * 8);

		if (size < 64 || size > BUNDLE_CAPACITY - offset ||
		    crc32_ieee(bundle + offset, size) != expected) {
			return -EBADMSG;
		}
		slots[i]->size = size;
		slots[i]->data = bundle + offset;
		offset += ROUND_UP(size, 8);
	}
	return 0;
}

static int load_zapp(struct zapp *app)
{
	if (app->extension) {
		return 0;
	}
	if (!app->data) {
		return -ENOENT;
	}
	struct llext_buf_loader loader = LLEXT_TEMPORARY_BUF_LOADER(app->data, app->size);
	struct llext_load_param parameters = LLEXT_LOAD_PARAM_DEFAULT;
	int result = llext_load(&loader.loader, app->name, &app->extension, &parameters);
	if (result < 0) {
		return result;
	}
	app->render = llext_find_sym(&app->extension->exp_tab, "zapp_render");
	app->tick = llext_find_sym(&app->extension->exp_tab, "zapp_tick");
	if (!app->render || !app->tick) {
		llext_unload(&app->extension);
		return -ENOEXEC;
	}
	return 0;
}

static void gui(void *one, void *two, void *three)
{
	ARG_UNUSED(one);
	ARG_UNUSED(two);
	ARG_UNUSED(three);
	const struct device *display = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

	if (!device_is_ready(display) || !lv_display_get_default()) {
		printk("RPiduino HDMI: framebuffer unavailable\n");
		return;
	}
	int result = locate_zapps();
	if (result == 0) {
		result = load_zapp(&desktop);
	}
	if (result < 0) {
		printk("RPiduino ZAPP: desktop load failed (%d)\n", result);
		return;
	}
	result = display_blanking_off(display);
	if (result < 0 && result != -ENOSYS) {
		printk("RPiduino HDMI: unblank failed (%d)\n", result);
		return;
	}
	zapp_input_init();
	lv_display_add_event_cb(lv_display_get_default(), redraw_complete, LV_EVENT_RENDER_READY,
				NULL);

	desktop.render(&ui, ZAPP_SPLASH);
	printk("RPiduino ZAPP: splash\n");
	int64_t splash_until = k_uptime_get() + 2000;
	struct zapp *active = &desktop;
	for (;;) {
		const int64_t now = k_uptime_get();
		if (splash_until && now >= splash_until) {
			splash_until = 0;
			desktop.render(&ui, ZAPP_DESKTOP);
			printk("RPiduino ZAPP: desktop ready\n");
		}
		if (!splash_until) {
			enum zapp_page page = atomic_set(&pending_page, 0);
			if (page >= ZAPP_DESKTOP && page <= ZAPP_BENCHMARK) {
				struct zapp *apps[] = {&desktop, &demo, &benchmark};
				struct zapp *next = apps[page - ZAPP_DESKTOP];

				result = load_zapp(next);
				if (result < 0) {
					printk("RPiduino ZAPP: %s load failed (%d)\n", next->name, result);
				} else {
					active = next;
					active->render(&ui, page);
					printk("RPiduino ZAPP: opened %s\n", active->name);
				}
			}
		}
		active->tick((uint32_t)now);
		lv_timer_handler();
		k_sleep(K_MSEC(20));
	}
}

K_THREAD_DEFINE(zapp_gui, 8192, gui, NULL, NULL, NULL, 5, 0, 0);
