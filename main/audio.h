#include <opus.h>
#include <esp_timer.h>

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

MessageBufferHandle_t audio_buffer = nullptr;
uint8_t *audio_buffer_storage = NULL;
StaticStreamBuffer_t audio_buffer_ctrl;

M5AtomS3 *board = nullptr;
OpusCoder *opus_coder = nullptr;

class PacedAudioPlayer {
public:
  static void playback_task(void *arg) {
    uint8_t opus_buffer[OPUS_BUFFER_SIZE];
    auto decoder_buffer = (opus_int16 *)malloc(PCM_BUFFER_SIZE);
    assert(decoder_buffer != nullptr);

    for (;;) {
      auto size = xMessageBufferReceive(audio_buffer, opus_buffer, sizeof(opus_buffer), portMAX_DELAY);
      assert(size != 0);

      auto decoded_size = opus_coder->Decode(opus_buffer, size, decoder_buffer);
      assert(decoded_size != 0);

      board->PlayAudio((uint8_t *) decoder_buffer, PCM_BUFFER_SIZE);
    }
  }

  PacedAudioPlayer() {
    auto size = 1024 * 1024;
    audio_buffer_storage = (uint8_t*) malloc(size);
    assert(audio_buffer_storage != nullptr);

    audio_buffer = xMessageBufferCreateStatic(size, audio_buffer_storage, &audio_buffer_ctrl);
    assert(audio_buffer != nullptr);

    xTaskCreate(playback_task, "playback_task", 16384, NULL, 5, NULL);
  }

  void Queue(uint8_t *data, size_t size) {
    assert(xMessageBufferSend(audio_buffer, data, size, 0) == size);
  }
};
