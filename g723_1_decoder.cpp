#include "g723_1_decoder.h"

extern "C" {
    #include "g723_1/TYPEDEF2.H"
    #include "g723_1/CST2.H"
    #include "g723_1/DECOD2.H"
}

#define G723_FRAME_SIZE_63 24
#define G723_FRAME_SIZE_53 20
#define G723_FRAME_SIZE_SID 4

static DecStatVal g_itu_decoder_state;

void g723_init_decoder() {
    std::memset(&g_itu_decoder_state, 0, sizeof(DecStatVal));
}

void g723_reset_decoder() {
    std::memset(&g_itu_decoder_state, 0, sizeof(DecStatVal));
}

int g723_decode_frame(const unsigned char* input, float* output_pcm) {
    unsigned char first_byte = input[0];
    int current_frame_size = 0;
    int crnt_crate = 0;

    switch (first_byte & 0x03) {
        case 0x00: current_frame_size = G723_FRAME_SIZE_63; crnt_crate = 0; break;
        case 0x01: current_frame_size = G723_FRAME_SIZE_53; crnt_crate = 1; break;
        case 0x02: current_frame_size = G723_FRAME_SIZE_SID; crnt_crate = 2; break;
        default:   current_frame_size = 1; crnt_crate = 3; break; // PLC
    }

    // Вызов оригинального референсного ядра ITU-T
    Decod(&g_itu_decoder_state, output_pcm, (char*)input, crnt_crate);

    return current_frame_size;
}
