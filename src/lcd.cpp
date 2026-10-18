#include "lcd.h"

#include <Arduino.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_rgb.h>
#include <freertos/semphr.h>

namespace lcd {
namespace {

constexpr int PIN_BACKLIGHT = 2;
constexpr int BACKLIGHT_PWM_HZ = 20000;

// LovyanGFX speichert RGB565 mit vertauschten Bytes (RRRRRGGG GGGBBBBB). Statt jedes Pixel
// umzudrehen, sind die Datenleitungen passend dazu angeordnet.
constexpr int DATA_PINS[16] = {
    15, 16, 4,           // G3 G4 G5
    45, 48, 47, 21, 14,  // R0 R1 R2 R3 R4
    8,  3,  46, 9,  1,   // B0 B1 B2 B3 B4
    5,  6,  7,           // G0 G1 G2
};

esp_lcd_panel_handle_t panel = nullptr;
void* frameBuffers[2];
int back = 1;
SemaphoreHandle_t frameDone;
LGFX_Sprite sprite;

bool IRAM_ATTR onFrameDone(esp_lcd_panel_handle_t, const esp_lcd_rgb_panel_event_data_t*, void*) {
  BaseType_t woken = pdFALSE;
  xSemaphoreGiveFromISR(frameDone, &woken);
  return woken == pdTRUE;
}

void attachCanvas() { sprite.setBuffer(frameBuffers[back], WIDTH, HEIGHT, 16); }

}  // namespace

bool begin() {
  esp_lcd_rgb_panel_config_t cfg = {};
  cfg.clk_src = LCD_CLK_SRC_DEFAULT;
  cfg.timings.pclk_hz = 16 * 1000 * 1000;
  cfg.timings.h_res = WIDTH;
  cfg.timings.v_res = HEIGHT;
  cfg.timings.hsync_pulse_width = 4;
  cfg.timings.hsync_back_porch = 16;
  cfg.timings.hsync_front_porch = 8;
  cfg.timings.vsync_pulse_width = 4;
  cfg.timings.vsync_back_porch = 4;
  cfg.timings.vsync_front_porch = 4;
  cfg.timings.flags.pclk_active_neg = 1;
  cfg.data_width = 16;
  cfg.bits_per_pixel = 16;
  cfg.num_fbs = 2;
  // Die CPU kopiert jeweils 10 Zeilen aus dem PSRAM in internen Speicher, von dort liest das Display
  cfg.bounce_buffer_size_px = WIDTH * 10;
  cfg.hsync_gpio_num = 39;
  cfg.vsync_gpio_num = 41;
  cfg.de_gpio_num = 40;
  cfg.pclk_gpio_num = 42;
  cfg.disp_gpio_num = -1;
  for (int i = 0; i < 16; i++) cfg.data_gpio_nums[i] = DATA_PINS[i];
  cfg.flags.fb_in_psram = 1;

  if (esp_lcd_new_rgb_panel(&cfg, &panel) != ESP_OK) return false;

  frameDone = xSemaphoreCreateBinary();
  esp_lcd_rgb_panel_event_callbacks_t callbacks = {};
  callbacks.on_frame_buf_complete = onFrameDone;
  esp_lcd_rgb_panel_register_event_callbacks(panel, &callbacks, nullptr);

  esp_lcd_panel_reset(panel);
  esp_lcd_panel_init(panel);
  esp_lcd_rgb_panel_get_frame_buffer(panel, 2, &frameBuffers[0], &frameBuffers[1]);
  memset(frameBuffers[0], 0, WIDTH * HEIGHT * 2);
  memset(frameBuffers[1], 0, WIDTH * HEIGHT * 2);
  attachCanvas();

  ledcAttach(PIN_BACKLIGHT, BACKLIGHT_PWM_HZ, 8);
  return true;
}

LGFX_Sprite& canvas() { return sprite; }

void show() {
  // Mit einem der eigenen Bildspeicher als Quelle kopiert der Treiber nichts, sondern schaltet
  // beim nächsten Bildwechsel auf diesen Speicher um
  esp_lcd_panel_draw_bitmap(panel, 0, 0, WIDTH, HEIGHT, frameBuffers[back]);
  // Erst nach dem Umschalten ist der andere Speicher frei zum Zeichnen
  xSemaphoreTake(frameDone, 0);
  xSemaphoreTake(frameDone, pdMS_TO_TICKS(100));
  back ^= 1;
  attachCanvas();
}

void setBrightness(uint8_t value) { ledcWrite(PIN_BACKLIGHT, value); }

}  // namespace lcd
