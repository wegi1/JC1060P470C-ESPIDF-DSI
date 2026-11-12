#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "esp_lcd_touch_gt911.h"
#include "gt911_touch.h"

#define CONFIG_LCD_HRES 1024
#define CONFIG_LCD_VRES 600

static const char *TAG = "example";

esp_lcd_touch_handle_t tp;
esp_lcd_panel_io_handle_t tp_io_handle;

uint16_t touch_strength[1];
uint8_t touch_cnt = 0;

gt911_touch::gt911_touch(int8_t sda_pin, int8_t scl_pin, int8_t rst_pin, int8_t int_pin)
{
    _sda = sda_pin;
    _scl = scl_pin;
    _rst = rst_pin;
    _int = int_pin;
}

void gt911_touch::begin()
{
    // i2c_config_t i2c_conf = {
    //     .mode = I2C_MODE_MASTER,
    //     .sda_io_num = (gpio_num_t)_sda,
    //     .scl_io_num = (gpio_num_t)_scl,
    //     .sda_pullup_en = GPIO_PULLUP_ENABLE,
    //     .scl_pullup_en = GPIO_PULLUP_ENABLE,
    // };
    // i2c_conf.master.clk_speed = 400000; // 400kHz

    i2c_master_bus_handle_t i2c_handle = NULL;

    if (i2c_master_get_bus_handle(I2C_NUM_0, &i2c_handle) != ESP_OK)
    {
        i2c_master_bus_config_t bus_cfg = {};
        bus_cfg.clk_source = I2C_CLK_SRC_DEFAULT;
        bus_cfg.i2c_port = I2C_NUM_0;
        bus_cfg.scl_io_num = (gpio_num_t)_scl;
        bus_cfg.sda_io_num = (gpio_num_t)_sda;
        bus_cfg.glitch_ignore_cnt = 7;
        bus_cfg.flags.enable_internal_pullup = true;
        ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &i2c_handle));
    }

    esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    tp_io_config.scl_speed_hz = 400000;

    ESP_LOGI(TAG, "Initialize touch IO (I2C)");
    esp_lcd_new_panel_io_i2c(i2c_handle, &tp_io_config, &tp_io_handle);

    esp_lcd_touch_config_t tp_cfg = {
        .x_max = CONFIG_LCD_HRES,
        .y_max = CONFIG_LCD_VRES,
        .rst_gpio_num = GPIO_NUM_NC,
        .int_gpio_num = (gpio_num_t)_int,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
    };
    // Sprzetowa sekwencja wyboru adresu 0x5D dla GT911 (INT=0 przy narastajacym RST)
    gpio_reset_pin((gpio_num_t)_rst);
    gpio_reset_pin((gpio_num_t)_int);
    gpio_set_direction((gpio_num_t)_rst, GPIO_MODE_OUTPUT);
    gpio_set_direction((gpio_num_t)_int, GPIO_MODE_OUTPUT);

    gpio_set_level((gpio_num_t)_int, 0);
    gpio_set_level((gpio_num_t)_rst, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level((gpio_num_t)_rst, 1);
    vTaskDelay(pdMS_TO_TICKS(60));
    gpio_set_direction((gpio_num_t)_int, GPIO_MODE_INPUT);

    ESP_LOGI(TAG, "Initialize touch controller gt911");
    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_gt911(tp_io_handle, &tp_cfg, &tp));
}

bool gt911_touch::getTouch(uint16_t *x, uint16_t *y)
{
    esp_lcd_touch_read_data(tp);
    bool touchpad_pressed = esp_lcd_touch_get_coordinates(tp, x, y, touch_strength, &touch_cnt, 1);

    return touchpad_pressed;
}

void gt911_touch::set_rotation(uint8_t r)
{
    switch (r)
    {
    case 0:
        esp_lcd_touch_set_swap_xy(tp, false);
        esp_lcd_touch_set_mirror_x(tp, false);
        esp_lcd_touch_set_mirror_y(tp, false);
        break;
    case 1:
        esp_lcd_touch_set_swap_xy(tp, false);
        esp_lcd_touch_set_mirror_x(tp, true);
        esp_lcd_touch_set_mirror_y(tp, true);
        break;
    case 2:
        esp_lcd_touch_set_swap_xy(tp, false);
        esp_lcd_touch_set_mirror_x(tp, false);
        esp_lcd_touch_set_mirror_y(tp, false);
        break;
    case 3:
        esp_lcd_touch_set_swap_xy(tp, false);
        esp_lcd_touch_set_mirror_x(tp, true);
        esp_lcd_touch_set_mirror_y(tp, true);
        break;
    }
}
