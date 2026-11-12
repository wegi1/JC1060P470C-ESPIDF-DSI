#include "rx8025t.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "pins_config.h"

static const char *TAG = "RX8025T";
static i2c_master_dev_handle_t rtc_dev_handle = NULL;

// Funkcje pomocnicze bcd2dec, dec2bcd oraz obsługa szyny I2C i rejestrów zegara RX8025T.
// Pełny kod implementacji sterownika można pobrać bezpośrednio ze źródła w serwisie Gist.

static uint8_t bcd2dec(uint8_t val)
{
    return ((val >> 4) * 10) + (val & 0x0F);
}

static uint8_t dec2bcd(uint8_t val)
{
    return ((val / 10) << 4) | (val % 10);
}

esp_err_t rx8025t_init(void)
{
    i2c_master_bus_handle_t bus_handle = NULL;
    if (i2c_master_get_bus_handle(I2C_NUM_0, &bus_handle) != ESP_OK)
    {
        i2c_master_bus_config_t bus_cfg = {
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .i2c_port = I2C_NUM_0,
            .scl_io_num = TOUCH_I2C_SCL_PIN,
            .sda_io_num = TOUCH_I2C_SDA_PIN,
            .glitch_ignore_cnt = 7,
            .flags.enable_internal_pullup = true,
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &bus_handle));
    }
    if (rtc_dev_handle == NULL)
    {
        i2c_device_config_t dev_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = RX8025T_ADDR,
            .scl_speed_hz = 100000,
        };
        ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_cfg, &rtc_dev_handle));
    }

    uint8_t ctrl1[2] = {RX8025T_REG_CTRL1, 0x20};
    esp_err_t err = i2c_master_transmit(rtc_dev_handle, ctrl1, sizeof(ctrl1), 1000);
    if (err != ESP_OK)
        return err;

    uint8_t ctrl2[2] = {RX8025T_REG_CTRL2, 0x00};
    err = i2c_master_transmit(rtc_dev_handle, ctrl2, sizeof(ctrl2), 1000);
    ESP_LOGI(TAG, "Konfiguracja rejestrow RX8025T zakonczona.");
    return err;
}

esp_err_t rx8025t_get_time(rx8025t_time_t *time)
{
    if (rtc_dev_handle == NULL)
        return ESP_ERR_INVALID_STATE;

    uint8_t reg = RX8025T_REG_SEC;
    uint8_t data[7] = {0};

    esp_err_t err = i2c_master_transmit_receive(rtc_dev_handle, &reg, 1, data, 7, 1000);
    if (err != ESP_OK)
        return err;

    time->second = bcd2dec(data[0] & 0x7F);
    time->minute = bcd2dec(data[1] & 0x7F);
    time->hour = bcd2dec(data[2] & 0x3F);
    time->weekday = data[3] & 0x07;
    time->day = bcd2dec(data[4] & 0x3F);
    time->month = bcd2dec(data[5] & 0x1F);
    time->year = bcd2dec(data[6]) + 2000;

    return ESP_OK;
}

esp_err_t rx8025t_set_time(const rx8025t_time_t *time)
{
    if (rtc_dev_handle == NULL)
        return ESP_ERR_INVALID_STATE;

    uint8_t payload[8] = {
        RX8025T_REG_SEC,
        dec2bcd(time->second),
        dec2bcd(time->minute),
        dec2bcd(time->hour),
        time->weekday & 0x07,
        dec2bcd(time->day),
        dec2bcd(time->month),
        dec2bcd(time->year % 100)};

    return i2c_master_transmit(rtc_dev_handle, payload, sizeof(payload), 1000);
}
