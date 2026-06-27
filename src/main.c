#include <pebble.h>

// ── Layout constants ────────────────────────────────────────────────────────

#define HAND_HOUR_THICKNESS   5
#define HAND_MIN_THICKNESS    3
#define HAND_SEC_THICKNESS    2
#define CENTER_DOT_RADIUS     5
#define TICK_MAJOR_LEN       10
#define TICK_MINOR_LEN        5

// AppMessage keys (must match pebble-js-app.js)
#define KEY_TEMPERATURE  0
#define KEY_CONDITIONS   1

// ── Globals ─────────────────────────────────────────────────────────────────

static Window   *s_window;
static Layer    *s_canvas;

// Sub-layers
static TextLayer *s_date_layer;
static TextLayer *s_weather_layer;
static TextLayer *s_batt_layer;

// State
static struct tm s_last_time;
static int       s_temperature  = 999;   // sentinel = not loaded yet
static char      s_conditions[32] = "";
static int       s_batt_pct     = 100;
static bool      s_charging     = false;
static bool      s_bt_connected = true;

// Buffers
static char s_date_buf[16];
static char s_weather_buf[24];
static char s_batt_buf[8];

// ── Helpers ─────────────────────────────────────────────────────────────────

static GPoint prv_polar(GPoint center, int32_t angle, int radius) {
  return GPoint(
    center.x + (int)(sin_lookup(angle) * radius / TRIG_MAX_RATIO),
    center.y - (int)(cos_lookup(angle) * radius / TRIG_MAX_RATIO)
  );
}

static void prv_draw_hand(GContext *ctx, GPoint center, int32_t angle,
                           int length, int thickness, GColor color) {
  graphics_context_set_stroke_color(ctx, color);
  graphics_context_set_stroke_width(ctx, thickness);
  GPoint tip = prv_polar(center, angle, length);
  graphics_draw_line(ctx, center, tip);
}

static void prv_draw_counterweight(GContext *ctx, GPoint center,
                                   int32_t angle, int length,
                                   int thickness, GColor color) {
  graphics_context_set_stroke_color(ctx, color);
  graphics_context_set_stroke_width(ctx, thickness);
  GPoint tail = prv_polar(center, angle + TRIG_MAX_ANGLE / 2, length);
  graphics_draw_line(ctx, center, tail);
}

// ── Canvas draw callback ─────────────────────────────────────────────────────

static void prv_canvas_update(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  GPoint center = grect_center_point(&bounds);
  int radius = (bounds.size.w < bounds.size.h
                ? bounds.size.w : bounds.size.h) / 2 - 4;

#ifdef PBL_COLOR
  GColor bg_color   = GColorBlack;
  GColor face_color = GColorDarkGray;
  GColor tick_color = GColorWhite;
  GColor hour_color = GColorWhite;
  GColor min_color  = GColorLightGray;
  GColor sec_color  = GColorRed;
  GColor dot_color  = GColorWhite;
  GColor bt_ok      = GColorGreen;
  GColor bt_bad     = GColorRed;
#else
  GColor bg_color   = GColorBlack;
  GColor face_color = GColorBlack;
  GColor tick_color = GColorWhite;
  GColor hour_color = GColorWhite;
  GColor min_color  = GColorWhite;
  GColor sec_color  = GColorLightGray;
  GColor dot_color  = GColorBlack;
  GColor bt_ok      = GColorWhite;
  GColor bt_bad     = GColorDarkGray;
#endif

  // ── Background ────────────────────────────────────────────────────────────
  graphics_context_set_fill_color(ctx, bg_color);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  // ── Watch face circle ─────────────────────────────────────────────────────
  graphics_context_set_fill_color(ctx, face_color);
  graphics_fill_circle(ctx, center, radius);

#ifdef PBL_COLOR
  graphics_context_set_stroke_color(ctx, GColorDarkGray);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_circle(ctx, center, radius);
#else
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_circle(ctx, center, radius);
#endif

  // ── Tick marks ────────────────────────────────────────────────────────────
  for (int i = 0; i < 60; i++) {
    int32_t angle = (TRIG_MAX_ANGLE * i) / 60;
    bool major = (i % 5 == 0);
    int  len   = major ? TICK_MAJOR_LEN : TICK_MINOR_LEN;
    int  thick = major ? 2 : 1;
    GPoint outer = prv_polar(center, angle, radius - 1);
    GPoint inner = prv_polar(center, angle, radius - 1 - len);
    graphics_context_set_stroke_color(ctx, tick_color);
    graphics_context_set_stroke_width(ctx, thick);
    graphics_draw_line(ctx, outer, inner);
  }

  // ── Bluetooth indicator (top centre) ─────────────────────────────────────
  {
    GColor bt_color = s_bt_connected ? bt_ok : bt_bad;
    GPoint bt_pos = GPoint(center.x, center.y - radius / 2);
    graphics_context_set_fill_color(ctx, bt_color);
    graphics_fill_circle(ctx, bt_pos, 4);
    if (!s_bt_connected) {
      // Strike-through to show disconnected
      graphics_context_set_stroke_color(ctx, bt_bad);
      graphics_context_set_stroke_width(ctx, 1);
      graphics_draw_line(ctx, GPoint(bt_pos.x - 4, bt_pos.y - 4),
                              GPoint(bt_pos.x + 4, bt_pos.y + 4));
    }
  }

  // ── Clock hands ───────────────────────────────────────────────────────────
  int32_t hour_angle = (TRIG_MAX_ANGLE *
    (((s_last_time.tm_hour % 12) * 60) + s_last_time.tm_min)) / (12 * 60);
  int32_t min_angle  = (TRIG_MAX_ANGLE * s_last_time.tm_min)  / 60;
  int32_t sec_angle  = (TRIG_MAX_ANGLE * s_last_time.tm_sec)  / 60;

  int hour_len = radius * 55 / 100;
  int min_len  = radius * 75 / 100;
  int sec_len  = radius * 80 / 100;

  // Hour hand + counterweight
  prv_draw_counterweight(ctx, center, hour_angle, radius * 15 / 100,
                         HAND_HOUR_THICKNESS, hour_color);
  prv_draw_hand(ctx, center, hour_angle, hour_len,
                HAND_HOUR_THICKNESS, hour_color);

  // Minute hand + counterweight
  prv_draw_counterweight(ctx, center, min_angle, radius * 12 / 100,
                         HAND_MIN_THICKNESS, min_color);
  prv_draw_hand(ctx, center, min_angle, min_len,
                HAND_MIN_THICKNESS, min_color);

  // Second hand + counterweight
  prv_draw_counterweight(ctx, center, sec_angle, radius * 20 / 100,
                         HAND_SEC_THICKNESS, sec_color);
  prv_draw_hand(ctx, center, sec_angle, sec_len,
                HAND_SEC_THICKNESS, sec_color);

  // Center dot
  graphics_context_set_fill_color(ctx, sec_color);
  graphics_fill_circle(ctx, center, CENTER_DOT_RADIUS);
  graphics_context_set_fill_color(ctx, dot_color);
  graphics_fill_circle(ctx, center, CENTER_DOT_RADIUS - 2);
}

// ── Text layer updates ───────────────────────────────────────────────────────

static void prv_update_date(void) {
  strftime(s_date_buf, sizeof(s_date_buf), "%a %d", &s_last_time);
  text_layer_set_text(s_date_layer, s_date_buf);
}

static void prv_update_weather(void) {
  if (s_temperature == 999) {
    snprintf(s_weather_buf, sizeof(s_weather_buf), "Loading...");
  } else {
    snprintf(s_weather_buf, sizeof(s_weather_buf), "%d° %s",
             s_temperature, s_conditions);
  }
  text_layer_set_text(s_weather_layer, s_weather_buf);
}

static void prv_update_battery(void) {
  if (s_charging) {
    snprintf(s_batt_buf, sizeof(s_batt_buf), "CHG");
  } else {
    snprintf(s_batt_buf, sizeof(s_batt_buf), "%d%%", s_batt_pct);
  }
  text_layer_set_text(s_batt_layer, s_batt_buf);
}

// ── Event handlers ───────────────────────────────────────────────────────────

static void prv_tick_handler(struct tm *tick_time, TimeUnits changed) {
  s_last_time = *tick_time;

  if (changed & DAY_UNIT) {
    prv_update_date();
  }

  layer_mark_dirty(s_canvas);

  // Request fresh weather every 30 minutes
  if (tick_time->tm_min % 30 == 0 && tick_time->tm_sec == 0) {
    DictionaryIterator *iter;
    if (app_message_outbox_begin(&iter) == APP_MSG_OK) {
      dict_write_uint8(iter, 0, 0);
      app_message_outbox_send();
    }
  }
}

static void prv_battery_handler(BatteryChargeState state) {
  s_batt_pct  = state.charge_percent;
  s_charging  = state.is_charging;
  prv_update_battery();
}

static void prv_bt_handler(bool connected) {
  s_bt_connected = connected;
  if (!connected) {
    vibes_short_pulse();
  }
  layer_mark_dirty(s_canvas);
}

static void prv_inbox_received(DictionaryIterator *iter, void *ctx) {
  Tuple *temp_t  = dict_find(iter, KEY_TEMPERATURE);
  Tuple *cond_t  = dict_find(iter, KEY_CONDITIONS);

  if (temp_t)  s_temperature = (int)temp_t->value->int32;
  if (cond_t)  snprintf(s_conditions, sizeof(s_conditions),
                        "%s", cond_t->value->cstring);

  prv_update_weather();
}

static void prv_inbox_dropped(AppMessageResult reason, void *ctx) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Inbox dropped: %d", (int)reason);
}

// ── Window lifecycle ──────────────────────────────────────────────────────────

static void prv_window_load(Window *window) {
  Layer  *root   = window_get_root_layer(window);
  GRect   bounds = layer_get_bounds(root);

  window_set_background_color(window, GColorBlack);

  // ── Canvas ────────────────────────────────────────────────────────────────
  s_canvas = layer_create(bounds);
  layer_set_update_proc(s_canvas, prv_canvas_update);
  layer_add_child(root, s_canvas);

  // ── Date label (bottom-left quadrant) ────────────────────────────────────
  GRect date_frame = GRect(bounds.size.w / 2 + 16,
                           bounds.size.h / 2 + 20,
                           60, 20);
  s_date_layer = text_layer_create(date_frame);
  text_layer_set_background_color(s_date_layer, GColorClear);
  text_layer_set_text_color(s_date_layer, GColorWhite);
  text_layer_set_font(s_date_layer,
                      fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD));
  text_layer_set_text_alignment(s_date_layer, GTextAlignmentLeft);
  layer_add_child(root, text_layer_get_layer(s_date_layer));

  // ── Weather label (bottom centre) ────────────────────────────────────────
  GRect weather_frame = GRect(4, bounds.size.h - 22,
                              bounds.size.w - 8, 20);
  s_weather_layer = text_layer_create(weather_frame);
  text_layer_set_background_color(s_weather_layer, GColorClear);
  text_layer_set_text_color(s_weather_layer, GColorWhite);
  text_layer_set_font(s_weather_layer,
                      fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_weather_layer, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_weather_layer));

  // ── Battery label (top-right) ─────────────────────────────────────────────
  GRect batt_frame = GRect(bounds.size.w / 2 + 16,
                           bounds.size.h / 2 - 40,
                           60, 20);
  s_batt_layer = text_layer_create(batt_frame);
  text_layer_set_background_color(s_batt_layer, GColorClear);
  text_layer_set_text_color(s_batt_layer, GColorWhite);
  text_layer_set_font(s_batt_layer,
                      fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_batt_layer, GTextAlignmentLeft);
  layer_add_child(root, text_layer_get_layer(s_batt_layer));

  // ── Initial state ─────────────────────────────────────────────────────────
  time_t now = time(NULL);
  s_last_time = *localtime(&now);
  prv_update_date();
  prv_update_weather();

  BatteryChargeState batt = battery_state_service_peek();
  s_batt_pct  = batt.charge_percent;
  s_charging  = batt.is_charging;
  prv_update_battery();

  s_bt_connected = connection_service_peek_pebble_app_connection();
}

static void prv_window_unload(Window *window) {
  layer_destroy(s_canvas);
  text_layer_destroy(s_date_layer);
  text_layer_destroy(s_weather_layer);
  text_layer_destroy(s_batt_layer);
}

// ── Init / Deinit ─────────────────────────────────────────────────────────────

static void init(void) {
  // AppMessage
  app_message_register_inbox_received(prv_inbox_received);
  app_message_register_inbox_dropped(prv_inbox_dropped);
  app_message_open(256, 256);

  // Window
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers){
    .load   = prv_window_load,
    .unload = prv_window_unload,
  });
  window_stack_push(s_window, true);

  // Services
  tick_timer_service_subscribe(SECOND_UNIT, prv_tick_handler);
  battery_state_service_subscribe(prv_battery_handler);
  connection_service_subscribe((ConnectionHandlers){
    .pebble_app_connection_handler = prv_bt_handler,
  });
}

static void deinit(void) {
  tick_timer_service_unsubscribe();
  battery_state_service_unsubscribe();
  connection_service_unsubscribe();
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
