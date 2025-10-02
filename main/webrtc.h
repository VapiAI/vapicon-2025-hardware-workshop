#include <cstring>
#include <string>
#include <string_view>

#include "cJSON.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "opus-coder.h"

#define TICK_INTERVAL 15
#define PCM_BUFFER_SIZE 640
#define OPUS_BUFFER_SIZE 1276  // 1276 bytes is recommended by opus_encode

PeerConnection *peer_connection = NULL;
M5AtomS3 *board = nullptr;
OpusCoder *opus_coder = nullptr;

StaticTask_t send_audio_task_buffer;
void send_audio_task(void *user_data) {
  auto read_buffer =
      (int16_t *)heap_caps_malloc(PCM_BUFFER_SIZE, MALLOC_CAP_DEFAULT);
  auto encoder_output_buffer = (uint8_t *)malloc(OPUS_BUFFER_SIZE);

  while (1) {
    int64_t start_us = esp_timer_get_time();

    if (board->IsButtonPressed()) {
      board->RecordAudio(read_buffer, PCM_BUFFER_SIZE);
      auto encoded_size =
          opus_coder->Encode(read_buffer, encoder_output_buffer);
      peer_connection_send_audio(peer_connection, encoder_output_buffer,
                                 encoded_size);
    }

    int64_t elapsed_us = esp_timer_get_time() - start_us;
    int64_t ms_sleep = TICK_INTERVAL - (elapsed_us / 1000);

    if (ms_sleep > 0) {
      vTaskDelay(pdMS_TO_TICKS(ms_sleep));
    }
  }
}

std::string filterCandidates(const std::string &sdp) {
  std::string out;
  out.reserve(sdp.size());
  bool keptRelay = false;

  size_t pos = 0;
  const size_t n = sdp.size();

  while (pos < n) {
    size_t end = sdp.find('\n', pos);
    if (end == std::string::npos)
      end = n;

    std::string_view line(&sdp[pos], end - pos);

    const bool isCandidate =
        line.size() >= 11 && line.rfind("a=candidate", 0) == 0;
    if (isCandidate) {
      if (!keptRelay && line.find("typ relay raddr") != std::string::npos) {
        out.append(line.data(), line.size());
        out.push_back('\n');
        keptRelay = true;
      }
    } else {
      out.append(line.data(), line.size());
      out.push_back('\n');
    }

    pos = (end < n) ? end + 1 : end;
  }

  return out;
}

opus_int16 *decoder_buffer = NULL;

void webrtc_create(M5AtomS3 *b) {
  vTaskPrioritySet(xTaskGetCurrentTaskHandle(), 10);
  board = b;
  opus_coder = new OpusCoder();
  peer_init();

  decoder_buffer = (opus_int16 *)malloc(PCM_BUFFER_SIZE);
  assert(decoder_buffer != nullptr);

  PeerConfiguration peer_connection_config = {
      .ice_servers = {{.urls = "stun:stun.cloudflare.com:3478",
                       .username = nullptr,
                       .credential = nullptr}},
      .audio_codec = CODEC_OPUS,
      .video_codec = CODEC_NONE,
      .datachannel = DATA_CHANNEL_STRING,
      .onaudiotrack = [](uint8_t *data, size_t size, void *userdata) -> void {
        auto decoded_size = opus_coder->Decode(data, size, decoder_buffer);
        if (decoded_size > 0) {
          board->PlayAudio(decoder_buffer, PCM_BUFFER_SIZE);
        }
      },
      .onvideotrack = NULL,
      .on_request_keyframe = NULL,
      .user_data = NULL,
  };

  peer_connection = peer_connection_create(&peer_connection_config);
  assert(peer_connection != NULL);

  peer_connection_oniceconnectionstatechange(
      peer_connection, [](PeerConnectionState state, void *user_data) -> void {
        ESP_LOGI("WebRTC", "state %s", peer_connection_state_to_string(state));
        if (state == PEER_CONNECTION_CONNECTED) {
          board->ShowVAPILogo();
          StackType_t *stack_memory = (StackType_t *)heap_caps_malloc(
              30000 * sizeof(StackType_t), MALLOC_CAP_SPIRAM);
          assert(stack_memory != nullptr);
          xTaskCreateStaticPinnedToCore(send_audio_task, "audio_publisher",
                                        30000, peer_connection, 7, stack_memory,
                                        &send_audio_task_buffer, 0);
        } else if (state == PEER_CONNECTION_CLOSED || state == PEER_CONNECTION_FAILED || state == PEER_CONNECTION_DISCONNECTED) {
          esp_restart();
        }
      });

  const char *offer = peer_connection_create_offer(peer_connection);
  std::string answer(HTTP_BUFFER_SIZE, '\0');

  board->ShowLogs("VAPI Signaling");
  do_http_request(offer, (char *)answer.c_str());
  answer = filterCandidates(answer);
  peer_connection_set_remote_description(peer_connection, answer.c_str(),
                                         SDP_TYPE_ANSWER);

  board->ShowLogs("WebRTC Connecting");

  while (true) {
    peer_connection_loop(peer_connection);
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}
