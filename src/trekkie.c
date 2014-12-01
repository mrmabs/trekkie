#include <pebble.h>

static Window *window;
static TextLayer *text_date_layer;
static TextLayer *text_nice_date_layer;
static TextLayer *text_time_layer;
static TextLayer *text_ampm_layer;
static TextLayer *text_stardate_layer;
static GBitmap *background_image;
static BitmapLayer *background_image_layer;
static GBitmap *battery_charge_image;
static BitmapLayer *battery_charge_image_layer;
static TextLayer *battery_percent;
static BatteryChargeState old_charge_state;
static Layer *battery_status_layer;
static GBitmap *bluetooth_image;
static BitmapLayer *bluetooth_status_layer;
static GFont *LCARS17;
static GFont *LCARS36;
static GFont *LCARS60;

void update_battery_display(BatteryChargeState charge_state) {
  static char battery_percent_text[] = "000";
  snprintf(battery_percent_text, sizeof(battery_percent_text), "%02u", charge_state.charge_percent);
  if(charge_state.charge_percent<100) battery_percent_text[2] = '%';
  text_layer_set_text(battery_percent, battery_percent_text);
  layer_set_hidden(bitmap_layer_get_layer(battery_charge_image_layer), !charge_state.is_charging);
  old_charge_state = charge_state; // Set for battery percentage bar.
  layer_mark_dirty(battery_status_layer);
}

void update_bluetooth_status(bool connected) {
  layer_set_hidden(bitmap_layer_get_layer(bluetooth_status_layer), !connected);
}

void battery_status_layer_update(Layer* layer, GContext* ctx) {
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, GRect(0, 0, old_charge_state.charge_percent*19/100, 3), 0, 0);
}

void date_update(struct tm* tick_time, TimeUnits units_changed) {
  static char date_text[] = "00.00.00";
  static char nice_date_text[] = "Xxx. Xxx. 00";

  // Date Layers
  strftime(date_text, sizeof(date_text), "%d.%m.%y", tick_time);
  strftime(nice_date_text, sizeof(nice_date_text), "%a.%n%b.%n%d", tick_time);
  text_layer_set_text(text_date_layer, date_text);
  text_layer_set_text(text_nice_date_layer, nice_date_text);
}

void stardate_update(struct tm* tick_time, TimeUnits units_changed) {
  // We need to get text versions of the date/time info before we can proceed, this is a pain, Pebble
  static char stardate_text[] = "00000.00";
  static char year[] = "00"; // year for stardate
  static char doy[] = "000"; // day of year
  static char hour[] = "00";
  static char mins[] = "00";
  float stardate;
  float decdays; // decimal days
  float yearoffset; // days since 01/01/2000 before this year
  
  // Get the ascii versions of the data we require
  strftime(year, sizeof(year), "%y", tick_time);
  strftime(doy, sizeof(doy), "%j", tick_time);
  strftime(hour, sizeof(hour), "%H", tick_time); // need to account for timezone!
  strftime(mins, sizeof(mins), "%M", tick_time);
  // calculate the part of year value (assumption: 1 year = 1000 stardates)
  yearoffset = (atoi(year) * 365) + (int)((atoi(year) - 1) / 4); // days per year, accounting for leap days since 2000. This will not work as intended after the year 2100. The "- 1" takes account of leap years /after/ the leap year is over
  decdays = ((((atoi(mins) / 60.0) + atoi(hour)) / 24.0) + atoi(doy) - 1);
  stardate = (yearoffset + decdays) * (1000 / 365.242196); // one exact year, time sync to atom clocks should account for leap seconds
  
  // StarDate Layer
  snprintf(stardate_text, sizeof(stardate_text), "%05d.%2d", (int)stardate, (int)(stardate * 1000) % 1000);
  text_layer_set_text(text_stardate_layer, stardate_text);
}

void time_update(struct tm* tick_time, TimeUnits units_changed) {
  if(units_changed>=DAY_UNIT) date_update(tick_time, units_changed);

  //StarDate - needs to be done before time and date are modified - needs to have utc sent to it, rather than localtime
  stardate_update(tick_time, units_changed);

  static char time_text[6];
  if (clock_is_24h_style()) {
    static const char *time_format="%R";
    strftime(time_text, sizeof(time_text), time_format, tick_time);
  }
  else {
    clock_copy_time_string(time_text, tick_time->tm_hour%12<10?5:6);
    text_layer_set_text(text_ampm_layer, tick_time->tm_hour<12?"am":"pm");
  }
  
  // Time layer
  text_layer_set_text(text_time_layer, time_text);
}

static void init(void) {
  window = window_create();
  window_stack_push(window, true /* Animated */);

  //Fonts
  LCARS17=fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_LCARS_BOLD_17));
  LCARS36=fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_LCARS_36));
  LCARS60=fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_LCARS_60));

  // Background Layer
  background_image_layer = bitmap_layer_create(layer_get_frame(window_get_root_layer(window)));
  background_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BACKGROUND);
  bitmap_layer_set_bitmap(background_image_layer, background_image);
  layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(background_image_layer));

  // Date Layer
  text_date_layer = text_layer_create(GRect(79, 5, 144-79, 168-5));
  text_layer_set_text_color(text_date_layer, GColorWhite);
  text_layer_set_background_color(text_date_layer, GColorClear);
  text_layer_set_font(text_date_layer, LCARS17);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(text_date_layer));

  // Nice Date Layer
  text_nice_date_layer = text_layer_create(GRect(6, 26, 144-6, 168-26));
  text_layer_set_background_color(text_nice_date_layer, GColorClear);
  text_layer_set_font(text_nice_date_layer, LCARS17);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(text_nice_date_layer));

  // Time Layer
  text_time_layer=text_layer_create(GRect(36, 12, 144-36, 168-12));
  text_layer_set_text_color(text_time_layer, GColorWhite);
  text_layer_set_background_color(text_time_layer, GColorClear);
  text_layer_set_font(text_time_layer, LCARS60);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(text_time_layer));

  // AM/PM Layer
  text_ampm_layer = text_layer_create(GRect(36, 69, 144-36, 168-69));
  text_layer_set_text_color(text_ampm_layer, GColorWhite);
  text_layer_set_background_color(text_ampm_layer, GColorClear);
  text_layer_set_font(text_ampm_layer, LCARS17);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(text_ampm_layer));

  // Stardate Layer
  text_stardate_layer = text_layer_create(GRect(36, 95, 144-36, 168-95));
  text_layer_set_text_color(text_stardate_layer, GColorWhite);
  text_layer_set_background_color(text_stardate_layer, GColorClear);
  text_layer_set_font(text_stardate_layer, LCARS36);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(text_stardate_layer));

  // Battery Percent Layer
  battery_percent = text_layer_create(GRect(8, 93, 27, 115));
  text_layer_set_background_color(battery_percent, GColorClear);
  text_layer_set_font(battery_percent, LCARS17);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(battery_percent));
  
  // Battery Charging Layer
  battery_charge_image_layer = bitmap_layer_create(GRect(8, 92, 19, 3));
  battery_charge_image=gbitmap_create_with_resource(RESOURCE_ID_BATTERY_CHARGING_IMAGE);
  bitmap_layer_set_bitmap(battery_charge_image_layer, battery_charge_image);
  layer_set_hidden(bitmap_layer_get_layer(battery_charge_image_layer), true);
  layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(battery_charge_image_layer));

  // Battery Status Layer
  battery_status_layer = layer_create(GRect(8, 92, 19, 3));
  layer_add_child(window_get_root_layer(window), battery_status_layer);
  layer_set_update_proc(battery_status_layer, battery_status_layer_update);

  // Bluetooth Status Layer
  bluetooth_status_layer = bitmap_layer_create(GRect(12, 111, 14, 13));
  bluetooth_image = gbitmap_create_with_resource(RESOURCE_ID_BLUETOOTH_IMAGE);
  bitmap_layer_set_bitmap(bluetooth_status_layer, bluetooth_image);
  layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(bluetooth_status_layer));

  // Prevent blank screen on init.
  time_t now = time(NULL);
  struct tm* tick_time;
  tick_time=localtime(&now);
  time_update(tick_time, DAY_UNIT);
  update_battery_display(battery_state_service_peek());
  update_bluetooth_status(bluetooth_connection_service_peek());

  // Subscribe to bluetooth, battery, and time updates.
  bluetooth_connection_service_subscribe(update_bluetooth_status);
  battery_state_service_subscribe(update_battery_display);
  tick_timer_service_subscribe(MINUTE_UNIT, &time_update);
}

static void deinit(void) {
  // Deinit, in reverse order from creation.

  // Destroy layers.
  bitmap_layer_destroy(bluetooth_status_layer);
  layer_destroy(battery_status_layer);
  bitmap_layer_destroy(battery_charge_image_layer);
  text_layer_destroy(battery_percent);
  text_layer_destroy(text_stardate_layer);
  text_layer_destroy(text_ampm_layer);
  text_layer_destroy(text_time_layer);
  text_layer_destroy(text_nice_date_layer);
  text_layer_destroy(text_date_layer);
  bitmap_layer_destroy(background_image_layer);

  // Destroy bitmaps.
  gbitmap_destroy(bluetooth_image);
  gbitmap_destroy(battery_charge_image);
  gbitmap_destroy(background_image);

  //Destroy fonts.
  fonts_unload_custom_font(LCARS17);
  fonts_unload_custom_font(LCARS36);
  fonts_unload_custom_font(LCARS60);
  
  // Destroy window.
  window_destroy(window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}

