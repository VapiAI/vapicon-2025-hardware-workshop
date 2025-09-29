#include <nvs_flash.h>
#include <peer.h>
#include <sdkconfig.h>

#include "http.h"
#include "m5-atom-s3.h"
#include "audio.h"
#include "webrtc.h"
#include "wifi.h"

extern "C" void app_main(void) {
  board = new M5AtomS3();
  opus_coder = new OpusCoder();

  board->ShowLogs("Connecting to Wifi");

  ESP_ERROR_CHECK(nvs_flash_init());
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  wifi_connect();

  board->ShowLogs("Start WebRTC");
  webrtc_create(board);
}
