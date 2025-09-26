#include <driver/gpio.h>
#include <driver/i2c_master.h>
#include <driver/i2s_std.h>
#include <driver/spi_master.h>
#include <esp_lcd_gc9a01.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_log.h>
#include <esp_lvgl_port.h>

#include "vapi-icon.h"

#define AUDIO_CODEC_I2C_SDA_PIN GPIO_NUM_38
#define AUDIO_CODEC_I2C_SCL_PIN GPIO_NUM_39

#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 128

// clang-format off
static const gc9a01_lcd_init_cmd_t gc9107_lcd_init_cmds[] = {
    {0xfe, (uint8_t[]){0x00}, 0, 0},
    {0xef, (uint8_t[]){0x00}, 0, 0},
    {0xb0, (uint8_t[]){0xc0}, 1, 0},
    {0xb2, (uint8_t[]){0x2f}, 1, 0},
    {0xb3, (uint8_t[]){0x03}, 1, 0},
    {0xb6, (uint8_t[]){0x19}, 1, 0},
    {0xb7, (uint8_t[]){0x01}, 1, 0},
    {0xac, (uint8_t[]){0xcb}, 1, 0},
    {0xab, (uint8_t[]){0x0e}, 1, 0},
    {0xb4, (uint8_t[]){0x04}, 1, 0},
    {0xa8, (uint8_t[]){0x19}, 1, 0},
    {0xb8, (uint8_t[]){0x08}, 1, 0},
    {0xe8, (uint8_t[]){0x24}, 1, 0},
    {0xe9, (uint8_t[]){0x48}, 1, 0},
    {0xea, (uint8_t[]){0x22}, 1, 0},
    {0xc6, (uint8_t[]){0x30}, 1, 0},
    {0xc7, (uint8_t[]){0x18}, 1, 0},
    {0xf0, (uint8_t[]){0x1f, 0x28, 0x04, 0x3e, 0x2a, 0x2e, 0x20, 0x00, 0x0c, 0x06, 0x00, 0x1c, 0x1f, 0x0f}, 14, 0},
    {0xf1, (uint8_t[]){0x00, 0x2d, 0x2f, 0x3c, 0x6f, 0x1c, 0x0b, 0x00, 0x00, 0x00, 0x07, 0x0d, 0x11, 0x0f}, 14, 0},
};
// clang-format on

class M5AtomS3 {
 public:
  i2c_master_bus_handle_t i2c_bus_;
  i2c_master_bus_handle_t i2c_bus_internal_;

  lv_obj_t *spinning_img = nullptr;

  M5AtomS3() {
    this->InitializeI2c();
    this->InitializeLp5562();
    this->InitializeSpi();
    this->InitializeGc9107Display();
  }

  static void spinner_set_angle(void *obj, int32_t v) {
    lv_image_set_rotation((lv_obj_t *)obj, v);
  }

 private:
  void InitializeI2c() {
    i2c_master_bus_config_t i2c_bus_cfg = {
        .i2c_port = I2C_NUM_1,
        .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
        .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags =
            {
                .enable_internal_pullup = 1,
                .allow_pd = 0,
            },
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));

    i2c_bus_cfg.i2c_port = I2C_NUM_0;
    i2c_bus_cfg.sda_io_num = GPIO_NUM_45;
    i2c_bus_cfg.scl_io_num = GPIO_NUM_0;
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_internal_));
  }

  void InitializeSpi() {
    spi_bus_config_t buscfg = {};
    buscfg.mosi_io_num = GPIO_NUM_21;
    buscfg.miso_io_num = GPIO_NUM_NC;
    buscfg.sclk_io_num = GPIO_NUM_15;
    buscfg.quadwp_io_num = GPIO_NUM_NC;
    buscfg.quadhd_io_num = GPIO_NUM_NC;
    buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
    ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
  }

  void InitializeLp5562() {
    i2c_device_config_t i2c_device_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = 0x30,
        .scl_speed_hz = 400 * 1000,
        .scl_wait_us = 0,
        .flags = {.disable_ack_check = 0},
    };
    i2c_master_dev_handle_t i2c_device;

    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus_internal_,
                                              &i2c_device_cfg, &i2c_device));
    assert(i2c_device != NULL);

    writeReg(i2c_device, 0x00, 0B01000000);  // Set chip_en to 1
    writeReg(i2c_device, 0x08, 0B00000001);  // Enable internal clock
    writeReg(i2c_device, 0x70, 0B00000000);  // Configure all LED outputs to be
                                             // controlled from I2C registers

    // PWM clock frequency 558 Hz
    writeReg(i2c_device, 0x08, readReg(i2c_device, 0x08) | 0B01000000);

    // Brightness 100%
    writeReg(i2c_device, 0x0E, 255);
    i2c_master_bus_rm_device(i2c_device);
  }

  void InitializeGc9107Display() {
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {};
    io_config.cs_gpio_num = GPIO_NUM_14;
    io_config.dc_gpio_num = GPIO_NUM_42;
    io_config.spi_mode = 0;
    io_config.pclk_hz = 40 * 1000 * 1000;
    io_config.trans_queue_depth = 10;
    io_config.lcd_cmd_bits = 8;
    io_config.lcd_param_bits = 8;
    ESP_ERROR_CHECK(
        esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &io_handle));

    esp_lcd_panel_handle_t panel_handle = NULL;
    gc9a01_vendor_config_t gc9107_vendor_config = {
        .init_cmds = gc9107_lcd_init_cmds,
        .init_cmds_size =
            sizeof(gc9107_lcd_init_cmds) / sizeof(gc9a01_lcd_init_cmd_t),
    };
    esp_lcd_panel_dev_config_t panel_config = {};
    panel_config.reset_gpio_num = GPIO_NUM_48;
    panel_config.rgb_endian = LCD_RGB_ENDIAN_BGR;
    panel_config.bits_per_pixel = 16;
    panel_config.vendor_config = &gc9107_vendor_config;

    ESP_ERROR_CHECK(
        esp_lcd_new_panel_gc9a01(io_handle, &panel_config, &panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    CreateLVGL(io_handle, panel_handle, DISPLAY_WIDTH, DISPLAY_HEIGHT);
  }

  void CreateLVGL(esp_lcd_panel_io_handle_t panel_io,
                  esp_lcd_panel_handle_t panel, int width, int height) {
    lv_init();

    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    lvgl_port_init(&port_cfg);

    const lvgl_port_display_cfg_t display_cfg = {
        .io_handle = panel_io,
        .panel_handle = panel,
        .control_handle = nullptr,
        .buffer_size = static_cast<uint32_t>(width * 20),
        .double_buffer = false,
        .trans_size = 0,
        .hres = static_cast<uint32_t>(width),
        .vres = static_cast<uint32_t>(height),
        .monochrome = false,
        .rotation =
            {
                .swap_xy = false,
                .mirror_x = false,
                .mirror_y = false,
            },
        .color_format = LV_COLOR_FORMAT_RGB565,
        .flags =
            {
                .buff_dma = 1,
                .buff_spiram = 0,
                .sw_rotate = 0,
                .swap_bytes = 1,
                .full_refresh = 0,
                .direct_mode = 0,
            },
    };

    auto display = lvgl_port_add_disp(&display_cfg);
    assert(display != nullptr);

    lvgl_port_lock(portMAX_DELAY);

    lv_display_set_offset(display, 0, 32);
    lv_obj_t *scr = lv_disp_get_scr_act(display);

    lv_obj_set_style_bg_color(scr, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

    spinning_img = lv_image_create(scr);
    lv_image_set_src(spinning_img, &vapi_icon);
    lv_obj_center(spinning_img);

    lv_image_set_pivot(spinning_img, vapi_icon.header.w / 2,
                       vapi_icon.header.h / 2);

    lv_image_set_antialias(spinning_img, true);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, spinning_img);
    lv_anim_set_exec_cb(&a, spinner_set_angle);
    lv_anim_set_duration(&a, 5000);
    lv_anim_set_values(&a, 0, 3600);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);

    lvgl_port_unlock();
  }

  void writeReg(i2c_master_dev_handle_t i2c_device, uint8_t reg,
                uint8_t value) {
    uint8_t buffer[2] = {reg, value};
    ESP_ERROR_CHECK(i2c_master_transmit(i2c_device, buffer, 2, 100));
  }

  uint8_t readReg(i2c_master_dev_handle_t i2c_device, uint8_t reg) {
    uint8_t buffer[1];
    ESP_ERROR_CHECK(
        i2c_master_transmit_receive(i2c_device, &reg, 1, buffer, 1, 100));
    return buffer[0];
  }
};
