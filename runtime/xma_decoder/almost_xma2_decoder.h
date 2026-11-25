#ifndef ALMOST_XMA2_DECODER_H
#define ALMOST_XMA2_DECODER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ALMOST_XMA2_PACKET_BYTES 2048u

typedef struct AlmostXma2Decoder AlmostXma2Decoder;
typedef struct AlmostXma2PcmBlock AlmostXma2PcmBlock;

enum AlmostXma2Status {
  ALMOST_XMA2_OK = 0,
  ALMOST_XMA2_INVALID_ARGUMENT = 1,
  ALMOST_XMA2_INVALID_FORMAT = 2,
  ALMOST_XMA2_BAD_PACKET_SIZE = 3,
  ALMOST_XMA2_OUT_OF_MEMORY = 4,
  ALMOST_XMA2_DECODER_FAILURE = 5,
  ALMOST_XMA2_PANIC = 6,
};

int32_t almost_xma2_decoder_create(uint32_t sample_rate, uint32_t channels,
                                    AlmostXma2Decoder **out_decoder);
int32_t almost_xma2_decode_buffer(AlmostXma2Decoder *decoder,
                                   const uint8_t *input, size_t input_len,
                                   AlmostXma2PcmBlock **out_block);
void almost_xma2_decoder_destroy(AlmostXma2Decoder *decoder);
const int16_t *almost_xma2_pcm_data(const AlmostXma2PcmBlock *block);
size_t almost_xma2_pcm_sample_count(const AlmostXma2PcmBlock *block);
void almost_xma2_pcm_destroy(AlmostXma2PcmBlock *block);

#ifdef __cplusplus
}
#endif

#endif
