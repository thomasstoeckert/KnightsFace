#include <pebble.h>
#include <pebble-events/pebble-events.h>
#include <pdc-transform/pdc-transform.h>

/* --- ARCHITECTURE FOR THIS WATCHFACE --- *\
 *  - Background is just the window's default color
 *  - Render ticks, icon graphics, text display, arms as one layer (that order)
 *  - Clay config to keep track of which icon is used & 1/0 for display text
 */


static Window *s_window;

static Layer *s_render_layer;

static char centerIcon[16] = "knightro";
static bool displayDate = true;
static bool displayCharge = true;
static bool displayAnalog = true;

static GFont s_data_font;
static GFont s_time_font;

// Config Stuff be here

void write_values(){
	persist_write_string(MESSAGE_KEY_centerIcon, centerIcon);
	
	persist_write_bool(MESSAGE_KEY_displayDate, displayDate);
	persist_write_bool(MESSAGE_KEY_displayCharge, displayCharge);
	persist_write_bool(MESSAGE_KEY_displayAnalog, displayAnalog);
}

void read_values(){
	char buffy[16];
	persist_read_string(MESSAGE_KEY_centerIcon, buffy, sizeof(buffy));
	strcpy(centerIcon, buffy);

	displayDate = persist_read_bool(MESSAGE_KEY_displayDate);
	displayCharge = persist_read_bool(MESSAGE_KEY_displayCharge);
	displayAnalog = persist_read_bool(MESSAGE_KEY_displayAnalog);
}

static void in_recieved_handler(DictionaryIterator *iter, void *context){
	(void) context;

	//Get Data
	Tuple *t = dict_read_first(iter);
	while(t != NULL) {
		uint32_t key = t->key;
		if (key == MESSAGE_KEY_centerIcon) { strcpy(centerIcon, t->value->cstring); }
		else if (key == MESSAGE_KEY_displayDate) { displayDate = (t->value->uint8 > 0); }
		else if (key == MESSAGE_KEY_displayCharge) { displayCharge = (t->value->uint8 > 0); }
		else if (key == MESSAGE_KEY_displayAnalog) { displayAnalog = (t->value->uint8 > 0); }
		t = dict_read_next(iter);
	}
	write_values();

	Layer *window_layer = window_get_root_layer(s_window);
	layer_mark_dirty(window_layer);
}

void render(Layer *layer, GContext *ctx) {
	GRect bounds = layer_get_bounds(layer);
	//GPoint center = grect_center_point(&bounds);
	GPoint home = bounds.origin;
	graphics_context_set_compositing_mode(ctx, GCompOpSet);
	time_t current_time = time(NULL);
	struct tm *time_tm = localtime(&current_time);
	/* ============== STEP 1: BACKGROUND & TICKS ================= */
	GBitmap *backmap = gbitmap_create_with_resource(RESOURCE_ID_BACKGROUND_TICKS);
	graphics_draw_bitmap_in_rect(ctx, backmap, bounds);
	gbitmap_destroy(backmap);
	/* ==================== STEP 2: DATA ========================= */
	GRect batteryrect = GRect(0, 26, 180, 32);
	GRect daterect = GRect(0, 138, 180, 32);
	graphics_context_set_text_color(ctx, GColorWhite);
	BatteryChargeState batteryPeek = battery_state_service_peek();
	if(displayCharge && !batteryPeek.is_charging) {
		char battBuff[6] = "000%%";
		snprintf(battBuff, sizeof(battBuff), "%d%%", batteryPeek.charge_percent);
		graphics_draw_text(ctx, battBuff, s_data_font, batteryrect, GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
	}
	if(displayDate) {
		char dateBuff[8] = "00 / 00";
		strftime(dateBuff, sizeof(dateBuff), "%m / %d", time_tm);
		graphics_draw_text(ctx, dateBuff, s_data_font, daterect, GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
	}
	/* ================= STEP 3: CENTER ICON ===================== */
	int iconid = RESOURCE_ID_BACKGROUND_TICKS;
	APP_LOG(APP_LOG_LEVEL_INFO, "CenterId: %s", centerIcon);
		 if (!strcmp(centerIcon, "knightro")) { iconid = RESOURCE_ID_CENTER_KNIGHTRO; APP_LOG(APP_LOG_LEVEL_INFO, "Knightro"); }
	else if (!strcmp(centerIcon, "stacked" )) { iconid = RESOURCE_ID_CENTER_STACKED; APP_LOG(APP_LOG_LEVEL_INFO, "Stacked"); }
	else if (!strcmp(centerIcon, "pegasus" )) { iconid = RESOURCE_ID_CENTER_PEGASUS; APP_LOG(APP_LOG_LEVEL_INFO, "Pegasus"); }
	GRect centerrect = GRect(42, 40, 100, 100);
	GBitmap *centermap = gbitmap_create_with_resource(iconid);
	graphics_draw_bitmap_in_rect(ctx, centermap, centerrect);
	gbitmap_destroy(centermap);
	/* ==================== STEP 4: ANALOG TIME ================== */
	if(displayAnalog) {
		int minute_angle = 360 * time_tm->tm_min / 60;
		int hour_angle = (360 * (((time_tm->tm_hour % 12) * 6) + (time_tm->tm_min / 10))) / (12 * 6);
		GDrawCommandImage *minute_image = gdraw_command_image_create_with_resource(RESOURCE_ID_SWORD_MINUTE);
		GDrawCommandImage *hour_image = gdraw_command_image_create_with_resource(RESOURCE_ID_SWORD_HOUR);
		pdc_transform_rotate_image(minute_image, minute_angle);
		pdc_transform_rotate_image(hour_image, hour_angle);
		gdraw_command_image_draw(ctx, minute_image, home);
		gdraw_command_image_draw(ctx, hour_image, home);
		gdraw_command_image_destroy(minute_image);
		gdraw_command_image_destroy(hour_image);
	} else {
		char minuteBuff[3] = "00";
		char hourBuff[3] = "00";
		strftime(minuteBuff, sizeof(minuteBuff), "%l", time_tm);
		strftime(hourBuff, sizeof(hourBuff), "%M", time_tm);
		GRect minutebox = GRect(26, 76, 50, 32);
		GRect hourbox = GRect(106, 76, 50, 32);
		graphics_context_set_text_color(ctx, GColorWhite);
		graphics_draw_text(ctx, minuteBuff, s_time_font, minutebox, GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
		graphics_draw_text(ctx, hourBuff, s_time_font, hourbox, GTextOverflowModeWordWrap, GTextAlignmentRight, NULL);
	}
}

void tick_handler(struct tm *tick_time, TimeUnits units_changed){
	Layer *window_layer = window_get_root_layer(s_window);
	layer_mark_dirty(window_layer);
}


static void window_load(Window *window) {
	// Honestly I don't think I've ever had a single project where I've not used this
	Layer *window_layer = window_get_root_layer(window);
	GRect bounds = layer_get_bounds(window_layer);

	read_values();

	s_render_layer = layer_create(bounds);
	layer_set_update_proc(s_render_layer, render);
	layer_add_child(window_layer, s_render_layer);

	tick_timer_service_subscribe((MINUTE_UNIT), tick_handler);
}

static void window_unload(Window *window) {
	layer_destroy(s_render_layer);
	tick_timer_service_unsubscribe();
}

static void init(void) {

	events_app_message_request_outbox_size(256);
	events_app_message_request_inbox_size(256);
	events_app_message_register_inbox_received(in_recieved_handler, NULL);
	events_app_message_open();

	s_window = window_create();
	window_set_window_handlers(s_window, (WindowHandlers) {
		.load = window_load,
		.unload = window_unload,
	});
	const bool animated = true;
	window_stack_push(s_window, animated);

	s_data_font = fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);
	s_time_font = fonts_get_system_font(FONT_KEY_LECO_20_BOLD_NUMBERS);
}

static void deinit(void) {
	window_destroy(s_window);
}

int main(void) {
	init();
	app_event_loop();
	deinit();
}
