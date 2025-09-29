#include <driver/gpio.h>
#include <driver/i2c_master.h>
#include <driver/i2s_std.h>
#include <driver/spi_master.h>
#include <esp_codec_dev.h>
#include <esp_codec_dev_defaults.h>
#include <esp_lcd_gc9a01.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_log.h>
#include <esp_lvgl_port.h>

#include "vapi-icon.h"

#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 128
#define BTN_PIN GPIO_NUM_41

#define SAMPLE_RATE (16000)

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
  M5AtomS3() {
    this->InitializeI2c();
    this->ConfigurePI4IOE();
    this->ConfigureLp5562();
    this->InitializeSpi();
    this->InitializeGc9107Display();
    this->ConfigureES8311();
    this->InitializeButton();
  }

  void ShowLogs(const char *log) {
    lvgl_port_lock(portMAX_DELAY);
    lv_label_set_text(label_, log);
    lvgl_port_unlock();
  }

  void ShowVAPILogo(void) {
    lvgl_port_lock(portMAX_DELAY);

    lv_obj_del(label_);

    lv_obj_t *scr = lv_disp_get_scr_act(display);

    lv_obj_set_style_bg_color(scr, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

    auto img = lv_image_create(scr);
    lv_image_set_src(img, &vapi_icon);
    lv_obj_center(img);

    lvgl_port_unlock();
  }

  bool IsButtonPressed(void) {
    return gpio_get_level(BTN_PIN) == 0;
  }

  void RecordAudio(void *dest, int size) {
    ESP_ERROR_CHECK_WITHOUT_ABORT(
        esp_codec_dev_read(audio_dev, (void *)dest, size));
  }

  void PlayAudio(void *data, int size) {
    ESP_ERROR_CHECK_WITHOUT_ABORT(
        esp_codec_dev_write(audio_dev, (void *)data, size));
  }

 private:
  i2c_master_bus_handle_t i2c_bus_;
  i2c_master_bus_handle_t i2c_bus_internal_;
  esp_codec_dev_handle_t audio_dev;

  lv_disp_t *display = nullptr;
  lv_obj_t *label_ = nullptr;

  void InitializeI2c() {
    i2c_master_bus_config_t i2c_bus_cfg = {
        .i2c_port = I2C_NUM_1,
        .sda_io_num = GPIO_NUM_38,
        .scl_io_num = GPIO_NUM_39,
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

  void ConfigureLp5562() {
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

    writeRegister(i2c_device, 0x00, 0B01000000);  // Set chip_en to 1
    writeRegister(i2c_device, 0x08, 0B00000001);  // Enable internal clock
    writeRegister(i2c_device, 0x70,
                  0B00000000);  // Configure all LED outputs to be
                                // controlled from I2C registers
    // PWM clock frequency 558 Hz
    writeRegister(i2c_device, 0x08,
                  readRegister(i2c_device, 0x08) | 0B01000000);

    // Brightness 100%
    writeRegister(i2c_device, 0x0E, 255);
  }

  void ConfigurePI4IOE() {
    i2c_master_dev_handle_t i2c_device;
    i2c_device_config_t i2c_device_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = 0x43,  // PI4IOE Address
        .scl_speed_hz = 400 * 1000,
        .scl_wait_us = 0,
        .flags =
            {
                .disable_ack_check = 0,
            },
    };
    ESP_ERROR_CHECK(
        i2c_master_bus_add_device(i2c_bus_, &i2c_device_cfg, &i2c_device));

    writeRegister(i2c_device, 0x07, 0x00);  // Set to high-impedance
    writeRegister(i2c_device, 0x0D, 0xFF);  // Enable pull-up
    writeRegister(i2c_device, 0x03, 0x6E);  // Set input=0, output=1
    writeRegister(i2c_device, 0x05, 0xFF);  // Unmute speaker
  }

  void ConfigureES8311(void) {
    i2s_chan_config_t chan_cfg = {
        .id = I2S_NUM_0,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = 6,
        .dma_frame_num = 240,
        .auto_clear_after_cb = true,
        .auto_clear_before_cb = false,
        .allow_pd = false,
        .intr_priority = 0,
    };

    i2s_chan_handle_t tx_handle = nullptr, rx_handle = nullptr;
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle, &rx_handle));

    i2s_std_config_t std_cfg = {
        .clk_cfg =
            {
                .sample_rate_hz = SAMPLE_RATE,
                .clk_src = I2S_CLK_SRC_DEFAULT,
                .ext_clk_freq_hz = 0,
                .mclk_multiple = I2S_MCLK_MULTIPLE_256,
                .bclk_div = false,
            },
        .slot_cfg = {.data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
                     .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
                     .slot_mode = I2S_SLOT_MODE_STEREO,
                     .slot_mask = I2S_STD_SLOT_BOTH,
                     .ws_width = I2S_DATA_BIT_WIDTH_16BIT,
                     .ws_pol = false,
                     .bit_shift = true,
                     .left_align = true,
                     .big_endian = false,
                     .bit_order_lsb = false},
        .gpio_cfg = {
            .mclk = GPIO_NUM_NC,
            .bclk = GPIO_NUM_8,
            .ws = GPIO_NUM_6,
            .dout = GPIO_NUM_5,
            .din = GPIO_NUM_7,
            .invert_flags = {
                .mclk_inv = false, .bclk_inv = false, .ws_inv = false}}};

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle, &std_cfg));

    audio_codec_i2s_cfg_t i2s_cfg = {
        .port = I2S_NUM_0,
        .rx_handle = rx_handle,
        .tx_handle = tx_handle,
    };
    audio_codec_i2c_cfg_t i2c_cfg = {
        .port = I2C_NUM_1,
        .addr = ES8311_CODEC_DEFAULT_ADDR,
        .bus_handle = i2c_bus_,
    };

    es8311_codec_cfg_t es8311_cfg;
    memset(&es8311_cfg, 0, sizeof(es8311_cfg));

    es8311_cfg.ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
    es8311_cfg.gpio_if = audio_codec_new_gpio();
    es8311_cfg.codec_mode = ESP_CODEC_DEV_WORK_MODE_BOTH;
    es8311_cfg.pa_pin = GPIO_NUM_NC;
    es8311_cfg.hw_gain.pa_voltage = 5.0;
    es8311_cfg.hw_gain.codec_dac_voltage = 3.3;

    esp_codec_dev_cfg_t dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_IN_OUT,
        .codec_if = es8311_codec_new(&es8311_cfg),
        .data_if = audio_codec_new_i2s_data(&i2s_cfg),
    };
    audio_dev = esp_codec_dev_new(&dev_cfg);

    esp_codec_dev_sample_info_t fs = {
        .bits_per_sample = 16,
        .channel = 1,
        .channel_mask = 0,
        .sample_rate = SAMPLE_RATE,
        .mclk_multiple = 0,
    };
    ESP_ERROR_CHECK(esp_codec_dev_open(audio_dev, &fs));
    ESP_ERROR_CHECK(esp_codec_dev_set_in_gain(audio_dev, 30.0));
    ESP_ERROR_CHECK(esp_codec_dev_set_out_vol(audio_dev, 100));
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

    display = lvgl_port_add_disp(&display_cfg);
    assert(display != nullptr);

    lvgl_port_lock(portMAX_DELAY);

    lv_display_set_offset(display, 0, 32);
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), 0);

    label_ = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_color(label_, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_obj_center(label_);

    lvgl_port_unlock();
  }

  void InitializeButton(void) {
    gpio_config_t io = {.pin_bit_mask = 1ULL << BTN_PIN,
                        .mode = GPIO_MODE_INPUT,
                        .pull_up_en = GPIO_PULLUP_ENABLE,
                        .pull_down_en = GPIO_PULLDOWN_DISABLE,
                        .intr_type = GPIO_INTR_DISABLE};
    ESP_ERROR_CHECK(gpio_config(&io));
  }

  void writeRegister(i2c_master_dev_handle_t i2c_device, uint8_t reg,
                     uint8_t value) {
    uint8_t buffer[2] = {reg, value};
    ESP_ERROR_CHECK(i2c_master_transmit(i2c_device, buffer, 2, 100));
  }

  uint8_t readRegister(i2c_master_dev_handle_t i2c_device, uint8_t reg) {
    uint8_t buffer[1];
    ESP_ERROR_CHECK(
        i2c_master_transmit_receive(i2c_device, &reg, 1, buffer, 1, 100));
    return buffer[0];
  }
};
