#include <windows.h>
#include <cstring>
#include "g723_1_decoder.h" // Интерфейс G.723.1
#include "g711u_coder.h"    // Интерфейс G.711 u-law

#define G723_SAMPLES_PER_FRAME 240
#define G723_FRAME_SIZE_SID 4

typedef struct blob_callback {
    short   (*blob_get_segment) (void*, char*, unsigned short, unsigned short*);
    void*   blob_handle;
    long    blob_number_segments;
    long    blob_max_segment;
    long    blob_total_length;
    void    (*blob_put_segment) (void*, const char*, unsigned short);
} *BLOB_CB;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        g711u_init_encoder(); // Инициализация таблицы PCMU
        g723_init_decoder();  // Инициализация глобального состояния декодера G.723.1
    }
    return TRUE;
}

extern "C" __declspec(dllexport) void __stdcall transcode_g723(BLOB_CB in_blob, BLOB_CB out_blob) { // void __stdcall или __cdecl
    if (!in_blob || !out_blob || !in_blob->blob_handle || !in_blob->blob_get_segment) {
        return;
    }

    // Сброс состояния декодера перед обработкой нового BLOB
    g723_reset_decoder();

    const unsigned short max_seg_size = 32768;
    char* input_chunk = new char[max_seg_size];
    char* output_chunk = new char[max_seg_size * G723_SAMPLES_PER_FRAME];

    unsigned short bytes_read = 0;
    int leftover_bytes = 0;
    int output_idx = 0;

    double pcm_output_buffer[G723_SAMPLES_PER_FRAME]; 

    while (in_blob->blob_get_segment(in_blob->blob_handle, input_chunk + leftover_bytes, max_seg_size - leftover_bytes, &bytes_read) == 0 || bytes_read > 0) {
        int total_valid_bytes = bytes_read + leftover_bytes;
        int input_idx = 0;

        while (input_idx < total_valid_bytes) {
            int bytes_left = total_valid_bytes - input_idx;
            
            if (bytes_left < G723_FRAME_SIZE_SID) {
                std::memmove(input_chunk, &input_chunk[input_idx], bytes_left);
                leftover_bytes = bytes_left;
                input_idx = total_valid_bytes;
                break;
            }

            // 1. Декодируем фрейм G.723.1 во float PCM
            int consumed_bytes = g723_decode_frame((const unsigned char*)&input_chunk[input_idx], pcm_output_buffer);
            input_idx += consumed_bytes;
            leftover_bytes = 0;

            // 2. Кодируем float PCM в G.711 u-law
            for (int i = 0; i < G723_SAMPLES_PER_FRAME; i++) {
                short sample_short = (short)pcm_output_buffer[i];
                
                // Вызов изолированного кодера
                output_chunk[output_idx++] = (char)g711u_linear_to_pcmu(sample_short);
                
                if (output_idx >= max_seg_size) {
                    out_blob->blob_put_segment(out_blob->blob_handle, output_chunk, output_idx);
                    output_idx = 0;
                }
            }
        }
    }

    if (output_idx > 0) {
        out_blob->blob_put_segment(out_blob->blob_handle, output_chunk, output_idx);
    }

    delete[] input_chunk;
    delete[] output_chunk;
}
