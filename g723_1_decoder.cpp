#include <cstring>
#include <new>
#include "g723_1_decoder.h"

#define __unix__
#define _single

extern "C" {
    #include "g723_1/typedef2.h"
    #include "g723_1/cst2.h"
    #include "g723_1/decod2.h"
}

#define G723_SAMPLES_PER_FRAME 240
#define G723_FRAME_SIZE_63 24
#define G723_FRAME_SIZE_53 20
#define G723_FRAME_SIZE_SID 4

struct G723DecoderContext {
    CRITICAL_SECTION critical_section;
    bool initialized = false;
};

bool g723_init_decoder() {
    Init_Decod();
    return true;
}

void g723_cleanup_decoder() {
}

G723DecoderContext* g723_create_context() {
    G723DecoderContext* ctx = new (std::nothrow) G723DecoderContext();
    if (!ctx) {
        return nullptr;
    }

    InitializeCriticalSection(&ctx->critical_section);
    ctx->initialized = true;

    EnterCriticalSection(&ctx->critical_section);
    Init_Decod();
    LeaveCriticalSection(&ctx->critical_section);

    return ctx;
}

void g723_destroy_context(G723DecoderContext* ctx) {
    if (!ctx) {
        return;
    }

    if (ctx->initialized) {
        DeleteCriticalSection(&ctx->critical_section);
        ctx->initialized = false;
    }

    delete ctx;
}

void g723_reset_decoder(G723DecoderContext* ctx) {
    if (!ctx || !ctx->initialized) {
        return;
    }

    EnterCriticalSection(&ctx->critical_section);
    Init_Decod();
    LeaveCriticalSection(&ctx->critical_section);
}

int g723_decode_frame(G723DecoderContext* ctx, const unsigned char* input, double* output_pcm) {
    if (!ctx || !ctx->initialized || !input || !output_pcm) {
        return 0;
    }

    unsigned char first_byte = *input;
    int current_frame_size = 0;
    int crnt_crate = 0;

    switch (first_byte & 0x03) {
        case 0x00: current_frame_size = G723_FRAME_SIZE_63; crnt_crate = 0; break;
        case 0x01: current_frame_size = G723_FRAME_SIZE_53; crnt_crate = 1; break;
        case 0x02: current_frame_size = G723_FRAME_SIZE_SID; crnt_crate = 2; break;
        default:   current_frame_size = 1; crnt_crate = 3; break;
    }

    FLOAT pcm_float_buffer[G723_SAMPLES_PER_FRAME];

    EnterCriticalSection(&ctx->critical_section);
    Decod(pcm_float_buffer, const_cast<char*>(reinterpret_cast<const char*>(input)), static_cast<Word16>(crnt_crate));
    LeaveCriticalSection(&ctx->critical_section);

    for (int i = 0; i < G723_SAMPLES_PER_FRAME; ++i) {
        output_pcm[i] = static_cast<double>(pcm_float_buffer[i]);
    }

    return current_frame_size;
}