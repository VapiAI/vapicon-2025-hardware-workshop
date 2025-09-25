#include <nvs_flash.h>
#include <peer.h>
#include <sdkconfig.h>

#include "m5-atom-s3.h"
#include "wifi.h"

extern "C" void app_main(void) {
  ESP_ERROR_CHECK(nvs_flash_init());
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  wifi_connect();
  peer_init();

  M5AtomS3 board;
}
