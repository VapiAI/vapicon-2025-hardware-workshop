#include <opus.h>

#define OPUS_BUFFER_SIZE 1276
#define OPUS_ENCODER_BITRATE 30000
#define OPUS_ENCODER_COMPLEXITY 0

#define PCM_BUFFER_SIZE 640
#define OPUS_BUFFER_SIZE 1276  // 1276 bytes is recommended by opus_encode

#define SAMPLE_RATE (16000)

#define TAG "audio"

class OpusCoder {
 public:
  OpusCoder() {
    int opus_error = 0;
    opus_decoder_ = opus_decoder_create(SAMPLE_RATE, 1, &opus_error);
    assert(opus_error == OPUS_OK);

    opus_encoder_ =
        opus_encoder_create(SAMPLE_RATE, 1, OPUS_APPLICATION_VOIP, &opus_error);
    assert(opus_error == OPUS_OK);
    assert(opus_encoder_init(opus_encoder_, SAMPLE_RATE, 1,
                             OPUS_APPLICATION_VOIP) == OPUS_OK);

    opus_encoder_ctl(opus_encoder_, OPUS_SET_BITRATE(OPUS_ENCODER_BITRATE));
    opus_encoder_ctl(opus_encoder_,
                     OPUS_SET_COMPLEXITY(OPUS_ENCODER_COMPLEXITY));
    opus_encoder_ctl(opus_encoder_, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
  }

  int Encode(const opus_int16 *read_buffer, uint8_t *encoder_output_buffer) {
    return opus_encode(opus_encoder_, read_buffer,
                       PCM_BUFFER_SIZE / sizeof(uint16_t),
                       encoder_output_buffer, OPUS_BUFFER_SIZE);
  }

  int Decode(uint8_t *data, size_t size, opus_int16 *decoder_buffer) {
    return opus_decode(opus_decoder_, data, size, decoder_buffer,
                       PCM_BUFFER_SIZE / sizeof(uint16_t), 0);
  }

 private:
  OpusDecoder *opus_decoder_ = nullptr;
  OpusEncoder *opus_encoder_ = nullptr;
};

QueueHandle_t audio_queue_;
M5AtomS3 *m5_board = nullptr;
uint8_t *audio_buffer = nullptr;

class PacedAudioPlayer {
public:
  static void playback_task(void *arg) {
    int32_t i;
    for (;;) {
      if (xQueueReceive(audio_queue_, &i, portMAX_DELAY) == pdPASS) {
        m5_board->PlayAudio(audio_buffer + (i * PCM_BUFFER_SIZE), PCM_BUFFER_SIZE);
        vTaskDelay(pdMS_TO_TICKS(20));
      }
    }
  }

  PacedAudioPlayer(M5AtomS3 *b) {
    m5_board = b;
    audio_buffer = (uint8_t*) calloc(PCM_BUFFER_SIZE * 3276, sizeof(uint8_t));
    audio_queue_ = xQueueCreate(3276, sizeof(int32_t));

    xTaskCreate(playback_task, "playback_task", 4096, NULL, 5, NULL);
  }

  void Queue(void *data) {
    memcpy(audio_buffer + (current_buffer_ * PCM_BUFFER_SIZE), data, PCM_BUFFER_SIZE);
    assert(xQueueSend(audio_queue_, &current_buffer_, portMAX_DELAY) == pdPASS);
    current_buffer_++;
    if (current_buffer_ >= 3275) {
      current_buffer_ = 0;
    }
  }

private:
  int32_t current_buffer_ = 0;
};
