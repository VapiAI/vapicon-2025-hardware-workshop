#include <opus.h>

#define OPUS_BUFFER_SIZE 1276
#define OPUS_ENCODER_BITRATE 30000
#define OPUS_ENCODER_COMPLEXITY 0

#define PCM_BUFFER_SIZE 640
#define OPUS_BUFFER_SIZE 1276  // 1276 bytes is recommended by opus_encode

#define SAMPLE_RATE (16000)

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
