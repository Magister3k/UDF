#include <memory>
#include "g723_1_decoder.h"
#include "g723_decoder.hpp"

struct G723DecoderContext {
    std::unique_ptr<g723_decoder::Decoder> decoder;
};

bool g723_init_decoder() {
    return true;
}

void g723_cleanup_decoder() {
}

G723DecoderContext* g723_create_context() {
    G723DecoderContext* ctx = new (std::nothrow) G723DecoderContext();
    if (!ctx) {
        return nullptr;
    }
    
    g723_decoder::DecoderConfig config;
    config.use_pf = true;
    ctx->decoder = std::make_unique<g723_decoder::Decoder>(config);
    
    return ctx;
}

void g723_destroy_context(G723DecoderContext* ctx) {
    if (ctx) {
        delete ctx;
    }
}

void g723_reset_decoder(G723DecoderContext* ctx) {
    if (ctx && ctx->decoder) {
        ctx->decoder->reset();
    }
}

int g723_decode_frame(G723DecoderContext* ctx, const unsigned char* input, double* output_pcm) {
    if (!ctx || !ctx->decoder || !input || !output_pcm) {
        return 0;
    }
    
    unsigned char first_byte = *input;
    g723_decoder::FrameType type;
    g723_decoder::CodecRate rate;
    int frame_size = 0;
    
    switch (first_byte & 0x03) {
        case 0x00: type = g723_decoder::FrameType::Active; rate = g723_decoder::CodecRate::Rate63; frame_size = 24; break;
        case 0x01: type = g723_decoder::FrameType::Active; rate = g723_decoder::CodecRate::Rate53; frame_size = 20; break;
        case 0x02: type = g723_decoder::FrameType::SID; rate = g723_decoder::CodecRate::Rate63; frame_size = 4; break;
        default:   type = g723_decoder::FrameType::Untransmitted; rate = g723_decoder::CodecRate::Rate63; frame_size = 1; break;
    }
    
    std::vector<uint8_t> data(frame_size);
    data[0] = first_byte;
    if (frame_size > 1) {
        for (int i = 1; i < frame_size; ++i) {
            data[i] = input[i];
        }
    }
    
    g723_decoder::BitstreamFrame frame;
    frame.data = std::move(data);
    frame.type = type;
    frame.rate = rate;
    frame.crc_error = false;
    
    auto result = ctx->decoder->decode(frame);
    if (!result) {
        return 0;
    }
    
    const g723_decoder::AudioFrame& audio = result.value();
    for (int i = 0; i < 240; ++i) {
        output_pcm[i] = static_cast<double>(audio.samples[i]);
    }
    
    return frame_size;
}