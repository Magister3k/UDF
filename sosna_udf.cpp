#include <windows.h>
#include <cstring>
#include <vector> // std::vector
#include <new>    // std::nothrow
#include "g723_1_decoder.h" // Интерфейс G.723.1
#include "g711u_coder.h"    // Интерфейс G.711 u-law

// Условная компиляция для отладочного вывода
#ifdef _DEBUG
#define DEBUG_OUTPUT(msg) OutputDebugStringA(msg)
#else
#define DEBUG_OUTPUT(msg)
#endif

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
        if (!g711u_init_encoder()) { // Инициализация таблицы PCMU
            DEBUG_OUTPUT("Failed to initialize G.711 encoder\n");
            return FALSE;
        }
        if (!g723_init_decoder()) {  // Инициализация глобального состояния декодера G.723.1
            DEBUG_OUTPUT("Failed to initialize G.723 decoder\n");
            return FALSE;
        }
    }
    else if (ul_reason_for_call == DLL_PROCESS_DETACH) {
        g723_cleanup_decoder(); // Очистка ресурсов декодера
    }
    return TRUE;
}

extern "C" __declspec(dllexport) void __cdecl transcode_g723(BLOB_CB in_blob, BLOB_CB out_blob) {
    if (!in_blob || !out_blob || !in_blob->blob_handle || !in_blob->blob_get_segment || !out_blob->blob_put_segment) {
        DEBUG_OUTPUT("Invalid BLOB callback or handle\n");
        return;
    }

    // Сброс состояния декодера перед обработкой нового BLOB
    g723_reset_decoder();

    const unsigned short max_seg_size = 32768;
    std::vector<char> input_chunk(max_seg_size);
    std::vector<char> output_chunk(max_seg_size * G723_SAMPLES_PER_FRAME);
    
    unsigned short bytes_read = 0;
    int leftover_bytes = 0;
    int output_idx = 0;
    size_t input_circular_pos = 0; // Позиция для циклического буфера

    double pcm_output_buffer[G723_SAMPLES_PER_FRAME];

    DEBUG_OUTPUT("Starting processing loop\n");
    
    while ((in_blob->blob_get_segment(in_blob->blob_handle, input_chunk.data() + leftover_bytes, max_seg_size - leftover_bytes, &bytes_read) == 0 && bytes_read > 0) || leftover_bytes > 0) {
        if (bytes_read == 0 && leftover_bytes == 0) {
            break; // Нет данных для обработки
        }
        
        int total_valid_bytes = bytes_read + leftover_bytes;
        
        while (input_circular_pos < total_valid_bytes) {
            int bytes_left = total_valid_bytes - input_circular_pos;
            
            if (bytes_left < G723_FRAME_SIZE_SID) {
                // Переносим оставшиеся байты в начало циклического буфера
                if (input_circular_pos > 0) {
                    std::memmove(input_chunk.data(), input_chunk.data() + input_circular_pos, bytes_left);
                }
                leftover_bytes = bytes_left;
                input_circular_pos = 0;
                break;
            }

            // 1. Декодируем фрейм G.723.1 во float PCM
            int consumed_bytes = g723_decode_frame(reinterpret_cast<const unsigned char*>(input_chunk.data() + input_circular_pos), pcm_output_buffer);
            if (consumed_bytes <= 0) {
                DEBUG_OUTPUT("Failed to decode G.723 frame\n");
                input_circular_pos += 1; // Пропустим некорректный байт
                leftover_bytes = 0;
                continue;
            }
            DEBUG_OUTPUT("Decoded G.723 frame\n");
            input_circular_pos += consumed_bytes;
            leftover_bytes = 0;

            // 2. Кодируем float PCM в G.711 u-law
            for (int i = 0; i < G723_SAMPLES_PER_FRAME; i++) {
                short sample_short = static_cast<short>(pcm_output_buffer[i]);
                output_chunk[output_idx++] = static_cast<char>(g711u_linear_to_pcmu(sample_short));
                
                if (output_idx >= max_seg_size) {
                    out_blob->blob_put_segment(out_blob->blob_handle, output_chunk.data(), output_idx);
                    output_idx = 0;
                }
            }
        }
        
        // Сбрасываем позицию циклического буфера, если данные обработаны
        if (leftover_bytes == 0) {
            input_circular_pos = 0;
        }
    }

    // Отправляем оставшиеся данные
    if (output_idx > 0) {
        out_blob->blob_put_segment(out_blob->blob_handle, output_chunk.data(), output_idx);
    }
}
