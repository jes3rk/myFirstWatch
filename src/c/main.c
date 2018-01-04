#include <pebble.h>

static Window *s_main_window;
static TextLayer *s_time_layer;
static TextLayer *s_weather_layer;

// Declare a global font
static GFont s_time_font;
static GFont s_weather_font;

// Stuff for a background
static BitmapLayer *s_background_layer;
static GBitmap *s_background_bitmap;

static void update_time() {
	// Get a tm structure
	time_t temp = time(NULL);
	struct tm *tick_time = localtime(&temp);
	
	// Write the current hours and minutes into a bugger
	static char s_buffer[8];
	strftime(s_buffer, sizeof(s_buffer), clock_is_24h_style() ? "%H:%M" : "%I:%M", tick_time);
	
	// Display this time on the text layer
	text_layer_set_text(s_time_layer, s_buffer);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
	update_time();
}

static void main_window_load(Window *window) {
	// Get information about the Window
	Layer *window_layer = window_get_root_layer(window);
	GRect bounds = layer_get_bounds(window_layer);
	
	// Build the Bitmap layer BEFORE the text layer to avoid hiding the textr
	// Create GBitmap
	s_background_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BACKGROUND);
	
	// Create BitmapLayer to display the GBitmap
	s_background_layer = bitmap_layer_create(bounds);
	
	// Set the bipmap onto the layer and add to the window
	bitmap_layer_set_bitmap(s_background_layer, s_background_bitmap);
	layer_add_child(window_layer, bitmap_layer_get_layer(s_background_layer));
	
	// Create the TextLayer with specific bounds
	s_time_layer = text_layer_create(
		GRect(0, PBL_IF_ROUND_ELSE(58, 52), bounds.size.w, 50));
	
	// Improve the layout to be more like a watchface
	text_layer_set_background_color(s_time_layer, GColorClear);
	text_layer_set_text_color(s_time_layer, GColorBlack);
	text_layer_set_text(s_time_layer, "00:00");
	
	// Create GFont
	s_time_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_PERFECT_DOS_48));
	
	// Apply the font to the text layer
	text_layer_set_font(s_time_layer, s_time_font);
	
	// Add it as a child layer to the Window's root layer
	layer_add_child(window_layer, text_layer_get_layer(s_time_layer));
	
	// Create temperature layer
	s_weather_layer = text_layer_create(
		GRect(0, PBL_IF_ROUND_ELSE(125, 120), bounds.size.w, 25));
	
	// Style the text
	text_layer_set_background_color(s_weather_layer, GColorClear);
	text_layer_set_text_color(s_weather_layer, GColorWhite);
	text_layer_set_text_alignment(s_weather_layer, GTextAlignmentCenter);
	text_layer_set_text(s_weather_layer, "Loading...");
	
	// Create second custom font, apply it and add to Window
	s_weather_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_PERFECT_DOS_20));
	text_layer_set_font(s_weather_layer, s_weather_font);
	layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_weather_layer));
}

static void main_window_unload(Window *window) {
	// Destroy text layer
	text_layer_destroy(s_time_layer);
	
	// Unload the font
	fonts_unload_custom_font(s_time_font);
	
	// Destroy GBitmap
	gbitmap_destroy(s_background_bitmap);
	
	// Destroy BitmapLayer
	bitmap_layer_destroy(s_background_layer);
	
	// Destroy weather elements
	text_layer_destroy(s_weather_layer);
	fonts_unload_custom_font(s_weather_font);
}

static void inbox_recieved_callback(DictionaryIterator *iterator, void *context) {
	static char temperature_buffer[8];
	static char conditions_buffer[32];
	static char weather_layer_buffer[32];
	
	// Read tuples for data
	Tuple *temp_tuple = dict_find(iterator, MESSAGE_KEY_TEMPERATURE);
	Tuple *conditions_tuple = dict_find(iterator, MESSAGE_KEY_CONDITIONS);
	
	// if all data is available, use it
	if(temp_tuple && conditions_tuple) {
		snprintf(temperature_buffer, sizeof(temperature_buffer), "%dC", (int)temp_tuple->value->int32);
		snprintf(conditions_buffer, sizeof(conditions_buffer), "%s", conditions_tuple->value->cstring);
		
		// Assemble ful string and display
		snprintf(weather_layer_buffer, sizeof(weather_layer_buffer), "%s, %s", temperature_buffer, conditions_buffer);
		text_layer_set_text(s_weather_layer, weather_layer_buffer);
	}
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped!");
}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed!");
}

static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
}

static void init() {
	// Create main Window element asnd assign to pointer
	s_main_window = window_create();
	
	// Set handlers to mainage the elements inside the Window
	window_set_window_handlers(s_main_window, (WindowHandlers) {
		.load = main_window_load,
		.unload = main_window_unload
	});
	
	// set the background color
	window_set_background_color(s_main_window, GColorBlack);
	
	// Show the Window on the watch, with animated=true
	window_stack_push(s_main_window, true);
	
	// Make sure the time is displayed from the start
	update_time();
	
	// Register with TickTimerService
	tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
	
	// Register callbacks
	app_message_register_inbox_received(inbox_recieved_callback);
	app_message_register_inbox_dropped(inbox_dropped_callback);
	app_message_register_outbox_failed(outbox_failed_callback);
	app_message_register_outbox_sent(outbox_sent_callback);
	
	// Open AppMessage
	const int inbox_size = 128;
	const int outbox_size = 128;
	app_message_open(inbox_size, outbox_size);
}

static void deinit() {
	window_destroy(s_main_window);
}

int main(void) {
	init();
	app_event_loop();
	deinit();
}
