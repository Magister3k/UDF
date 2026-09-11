#include <memory>

#include "g723_1_decoder.h"
#include "g723_decoder.hpp"

extern "C" {
    void Init_Decod(void);
    void Init_Dec_Cng(void);
}

struct G723DecoderContext {};

static G723DecoderContext g_static_context;
static std::unique_ptr<g723_decoder::Decoder> g_shared_decoder;

G723DecoderContext* __cdecl g723_create_context() {
    Init_Decod();
    Init_Dec_Cng();

    if (!g_shared_decoder) {
        g723_decoder::DecoderConfig config;
        config.use_pf = true;
        g_shared_decoder = std::make_unique<g723_decoder::Decoder>(config);
    } else {
        g_shared_decoder->reset();
    }

    return &g_static_context;
}

int g723_decode_frame(
    G723DecoderContext* context,
    const unsigned char* input,
    double* output_pcm) {
    (void)context;

    if (!g_shared_decoder || !input || !output_pcm) {
        return 0;
    }

    const unsigned char frame_type = input[0] & 0x03;
    const size_t frame_size = frame_type == 0x01 ? 20u :
                              frame_type == 0x02 ? 4u :
                              frame_type == 0x03 ? 1u : 24u;
    const g723_decoder::FrameType decoder_frame_type =
        frame_type == 0x01 ? g723_decoder::FrameType::Active :
        frame_type == 0x02 ? g723_decoder::FrameType::SID :
                             g723_decoder::FrameType::Untransmitted;
    const g723_decoder::CodecRate decoder_rate =
        frame_type == 0x01 ? g723_decoder::CodecRate::Rate53 :
                             g723_decoder::CodecRate::Rate63;

    const auto result = g_shared_decoder->decode(
        std::span<const uint8_t>(input, frame_size),
        decoder_frame_type,
        decoder_rate);
    if (!result.has_value()) {
        return 0;
    }

    const g723_decoder::AudioFrame& audio = result.value();
    for (size_t sample = 0; sample < audio.valid_samples; ++sample) {
        output_pcm[sample] = audio.samples[sample];
    }

    switch (frame_type) {
        case 0x00: return 24;
        case 0x01: return 20;
        case 0x02: return 4;
        default: return 1;
    }
}

void g723_destroy_context(G723DecoderContext* context) {
    (void)context;
}