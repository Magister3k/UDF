#include <cstring>
#include "g723_1_decoder.h"

// Локальное определение типа для совместимости с прототипом из DECOD2.H
typedef short Word16; 

extern "C" {
    #include "g723_1/decod2.h"
}

#define G723_FRAME_SIZE_63 24
#define G723_FRAME_SIZE_53 20
#define G723_FRAME_SIZE_SID 4

int g723_decode_frame(const unsigned char* input, double* output_pcm) {
    unsigned char first_byte = *input;
    int current_frame_size = 0;
    int crnt_crate = 0;

    switch (first_byte & 0x03) {
        case 0x00: current_frame_size = G723_FRAME_SIZE_63; crnt_crate = 0; break;
        case 0x01: current_frame_size = G723_FRAME_SIZE_53; crnt_crate = 1; break;
        case 0x02: current_frame_size = G723_FRAME_SIZE_SID; crnt_crate = 2; break;
        default:   current_frame_size = 1; crnt_crate = 3; break; // PLC
    }

    // Вызов оригинального референсного ядра ITU-T Annex B
    // Сигнатура из DECOD2.H: Decod(FLOAT *DataBuff, char *Vinp, Word16 Crc)
    Decod(output_pcm, (char*)input, (Word16)crnt_crate);

    return current_frame_size;
}
