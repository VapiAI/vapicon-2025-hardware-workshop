#include "cJSON.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <cstring>

#include <string>
#include <sstream>
#include <iostream>
#include <string>

#define TICK_INTERVAL 15
PeerConnection *peer_connection = NULL;

StaticTask_t send_audio_task_buffer;
void send_audio_task(void *user_data) {
  // auto peer_connection = (PeerConnection *)user_data;
  while (1) {
    int64_t start_us = esp_timer_get_time();

    // pull from board
    // encode via opus
    // send audio
    //reflect_send_audio(peer_connection);

    int64_t elapsed_us = esp_timer_get_time() - start_us;
    int64_t ms_sleep = TICK_INTERVAL - (elapsed_us / 1000);

    if (ms_sleep > 0) {
      vTaskDelay(pdMS_TO_TICKS(ms_sleep));
    }
  }
}


std::string filterCandidates(const std::string& sdp) {
    std::istringstream iss(sdp);
    std::ostringstream oss;
    std::string line;
    bool keptRelay = false;

    while (std::getline(iss, line)) {
        if (line.rfind("a=candidate", 0) == 0) {
            if (!keptRelay && line.find("typ relay raddr") != std::string::npos) {
                oss << line << "\n";
                keptRelay = true;
            }
        } else {
            oss << line << "\n";
        }
    }

    return oss.str();
}

void webrtc_create() {
  peer_init();

  PeerConfiguration peer_connection_config = {
      .ice_servers = {{.urls = "stun:stun.cloudflare.com:3478", .username = nullptr, .credential = nullptr}},
      .audio_codec = CODEC_OPUS,
      .video_codec = CODEC_NONE,
      .datachannel = DATA_CHANNEL_STRING,
      .onaudiotrack = [](uint8_t *data, size_t size, void *userdata) -> void {
      },
      .onvideotrack = NULL,
      .on_request_keyframe = NULL,
      .user_data = NULL,
  };

  peer_connection = peer_connection_create(&peer_connection_config);
  assert(peer_connection != NULL);

  peer_connection_oniceconnectionstatechange(
      peer_connection, [](PeerConnectionState state, void *user_data) -> void {
        if (state == PEER_CONNECTION_CONNECTED) {
          // StackType_t *stack_memory = (StackType_t *)heap_caps_malloc(
          //     30000 * sizeof(StackType_t), MALLOC_CAP_SPIRAM);
          // assert(stack_memory != nullptr);
          // xTaskCreateStaticPinnedToCore(
          //     reflect_send_audio_task, "audio_publisher", 30000,
          //     peer_connection, 7, stack_memory, &send_audio_task_buffer, 0);
        }
      });

  std::string answer(HTTP_BUFFER_SIZE, '\0');
  const char* offer = peer_connection_create_offer(peer_connection);
  do_http_request(offer, (char *) answer.c_str());

  answer = filterCandidates(answer);

  printf("\n ANSWER %s \n", answer.c_str());

  while (true) {
    peer_connection_loop(peer_connection);
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}
