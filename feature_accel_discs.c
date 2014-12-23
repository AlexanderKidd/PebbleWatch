/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  *
 * Project:    Tight Rope                                                 *
 * Author:     Alexander C. Kidd                                          *
 * Date:       12/22/14															                      *
 * Created originally as a fun game that also trains one's sense of       *
 * balance (or at least wrist handling) some time in November of 2014.    *
 * Disc code borrowed from Pebble feature_accel_discs.c example project.  *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  */
#include "pebble.h"

#define MATH_PI 3.141592653589793238462
#define NUM_DISCS 1
#define DISC_DENSITY 0.15
#define ACCEL_RATIO 0.05
#define ACCEL_STEP_MS 50

static GBitmap *background_bitmap;
static BitmapLayer *background_layer;
static TextLayer *score_text_layer;
static TextLayer *game_over_text_layer;

int score = 0;
int lastScore = 0;
int lastX = 0;
int lastZ = 0;
int buttonPress = 0;

bool destroy = false;
bool gameOver = false;

typedef struct Vec2d {
  double x;
  double y;
} Vec2d;

typedef struct Disc {
  Vec2d pos;
  Vec2d vel;
  double mass;
  double radius;
} Disc;

static Disc discs[NUM_DISCS];

static double next_radius = 15;

static Window *window;

static GRect window_frame;

static Layer *disc_layer;

static AppTimer *timer;

static void end_game() {
  //End game routines.
  vibes_double_pulse();
  gameOver = true;
  accel_data_service_unsubscribe();
  layer_destroy(disc_layer);
  gbitmap_destroy(background_bitmap);
  
  //Covers the area on screen below score_text_layer.
  Layer *window_layer = window_get_root_layer(window);
  layer_mark_dirty(window_layer);
  
  game_over_text_layer = text_layer_create(GRect(0, 25, 144, 143));
  text_layer_set_font(game_over_text_layer, fonts_get_system_font(FONT_KEY_DROID_SERIF_28_BOLD));
  text_layer_set_text(game_over_text_layer, "GAME OVER!");
  layer_add_child(window_layer, text_layer_get_layer(game_over_text_layer));
}

static double disc_calc_mass(Disc *disc) {
  return MATH_PI * disc->radius * disc->radius * DISC_DENSITY;
}

static void disc_init(Disc *disc) {
  GRect frame = window_frame;
  disc->pos.x = frame.size.w/2;
  disc->pos.y = frame.size.h/2;
  disc->vel.x = 0;
  disc->vel.y = 0;
  disc->radius = next_radius;
  disc->mass = disc_calc_mass(disc);
  next_radius += 0.5;
}

static void disc_apply_force(Disc *disc, Vec2d force) {
  disc->vel.x += force.x / disc->mass;
  disc->vel.y += force.y / disc->mass;
}

static void disc_apply_accel(Disc *disc, AccelData accel) {
  Vec2d force;
  force.x = accel.x * ACCEL_RATIO;
  force.y = -accel.y * ACCEL_RATIO;
  disc_apply_force(disc, force);
}

static void disc_update(Disc *disc) {
  const GRect frame = window_frame;
//  double e = 0.5;
  if ((disc->pos.x - disc->radius < 0 && disc->vel.x < 0)
    || (disc->pos.x + disc->radius > frame.size.w && disc->vel.x > 0)) {
//     disc->vel.x = -disc->vel.x * e;
    end_game();
  }
  if ((disc->pos.y - disc->radius < 0 && disc->vel.y < 0)
    || (disc->pos.y + disc->radius > frame.size.h && disc->vel.y > 0)) {
//     disc->vel.y = -disc->vel.y * e;
    end_game();
  }
  disc->pos.x += disc->vel.x;
  disc->pos.y += disc->vel.y;
}

static void disc_draw(GContext *ctx, Disc *disc) {
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_circle(ctx, GPoint(disc->pos.x, disc->pos.y), disc->radius);
}

static void disc_layer_update_callback(Layer *me, GContext *ctx) {
  for (int i = 0; i < NUM_DISCS; i++) {
    disc_draw(ctx, &discs[i]);
  }
}

static void update_game(int currX, int currZ) {
  if (abs(lastX - currX) >= 100 && abs(lastZ - currZ) >= 100) {
    score += 1;
  }
  
  lastX = currX;
  lastZ = currZ;
}

static void timer_callback(void *data) {
  if (gameOver == false) {
    AccelData accel = (AccelData) { .x = 0, .y = 0, .z = 0 };
  
    accel_service_peek(&accel);
    
    if (buttonPress != 0) {
      if(destroy == false) {
        bitmap_layer_destroy(background_layer);
        destroy = true;
      }
      
      update_game(accel.x, accel.z);
      
      if (lastScore != score) {
        static char buf[256] = "12345";
        snprintf(buf, sizeof(buf), "Score: %d", score);
        text_layer_set_text(score_text_layer, buf);
        lastScore = score;
      }
        
      for (int i = 0; i < NUM_DISCS; i++) {
        Disc *disc = &discs[i];
        disc_apply_accel(disc, accel);
        disc_update(disc);
      }
      
      layer_mark_dirty(disc_layer);
    }
  }
  
  timer = app_timer_register(ACCEL_STEP_MS, timer_callback, NULL);
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect frame = window_frame = layer_get_frame(window_layer);
  
  // Create GBitmap, then set to created BitmapLayer.
  background_bitmap = gbitmap_create_with_resource(RESOURCE_ID_TITLE_SCREEN);
  background_layer = bitmap_layer_create(frame);
  bitmap_layer_set_bitmap(background_layer, background_bitmap);
  layer_add_child(window_layer, bitmap_layer_get_layer(background_layer));
}

//Starts/restarts tight rope simulation.
static void start_game(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect frame = window_frame = layer_get_frame(window_layer);
  
  layer_mark_dirty(window_layer);
  
  disc_layer = layer_create(frame);
  layer_set_update_proc(disc_layer, disc_layer_update_callback);
  layer_add_child(window_layer, disc_layer);
  
  score_text_layer = text_layer_create(GRect(0, 0, 144, 25));
  text_layer_set_text(score_text_layer, "Score: 0");
  layer_add_child(window_layer, text_layer_get_layer(score_text_layer));

  for (int i = 0; i < NUM_DISCS; i++) {
    disc_init(&discs[i]);
  }
}

//Start game via up, down, or select buttons.
static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (buttonPress == 0) {
    start_game(window);
    buttonPress = 1;
  }
}

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (buttonPress == 0) {
    start_game(window);
    buttonPress = 1;
  }
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (buttonPress == 0) {
    start_game(window);
    buttonPress = 1;
  }
}

//Click to start game.
static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
}

static void window_unload(Window *window) {
  gbitmap_destroy(background_bitmap);
  text_layer_destroy(score_text_layer);
}

static void init(void) {
  light_enable(true);
  window = window_create();
  window_set_click_config_provider(window, click_config_provider);
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload
  });
  window_stack_push(window, true /* Animated */);
  window_set_background_color(window, GColorBlack);

  accel_data_service_subscribe(0, NULL);

  timer = app_timer_register(ACCEL_STEP_MS, timer_callback, NULL);
}

static void deinit(void) {
  window_destroy(window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
