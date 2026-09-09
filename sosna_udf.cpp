#include <windows.h>
#include <cstring>
#include <vector>
#include <new>
#include <algorithm>
#include "g723_1_decoder.h"
#include "g711u_coder.h"

#define G723_SAMPLES_PER_FRAME 240
#define G723_FRAME_SIZE_SID 4
#define G723_MAX_FRAME_SIZE 24
#define G723_MIN_FRAME_SIZE 1

typedef struct blob_callback {
    short   (*blob_get_segment) (void*, char*, unsigned short, unsigned short*);
    void*   blob_handle;
    long    blob_max_segment;
    void    (*blob_put_segment) (void*, const char*, unsigned short);
} *BLOB_CB;

static bool g_decoder_initialized = false;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    (void)hModule;
    (void)lpReserved;

    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        if (!g711u_init_encoder()) return FALSE;
        if (!g723_init_decoder()) return FALSE;
        g_decoder_initialized = true;
    }
    else if (ul_reason_for_call == DLL_PROCESS_DETACH) {
        g723_cleanup_decoder();
        g_decoder_initialized = false;
    }
    return TRUE;
}

static bool validate_blob_callbacks(const BLOB_CB in_blob, const BLOB_CB out_blob) {
    if (!in_blob || !out_blob) return false;
    if (!in_blob->blob_handle || !in_blob->blob_get_segment) return false;
    if (!out_blob->blob_put_segment) return false;
    if (in_blob->blob_max_segment < 0 || in_blob->blob_max_segment > 200000) return false;
    return true;
}

static inline void encode_pcm_to_pcmu(const double* pcm_input, unsigned char* output, int sample_count) {
    for (int i = 0; i < sample_count; ++i) {
        short sample_short = static_cast<short>(pcm_input[i]);
        output[i] = g711u_linear_to_pcmu(sample_short);
    }
}

static inline void flush_output_buffer(BLOB_CB out_blob, std::vector<unsigned char>& output_buffer, size_t& output_idx) {
    if (output_idx > 0) {
        out_blob->blob_put_segment(out_blob->blob_handle, reinterpret_cast<const char*>(output_buffer.data()), static_cast<unsigned short>(output_idx));
        output_idx = 0;
    }
}

static void transcode_internal(BLOB_CB in_blob, BLOB_CB out_blob) {
    const unsigned int max_seg_size = 200000;

    std::vector<unsigned char> input_buffer(max_seg_size);
    std::vector<unsigned char> output_buffer(max_seg_size);
    double pcm_output[G723_SAMPLES_PER_FRAME];

    unsigned int buffer_data_size = 0;
    size_t output_idx = 0;
    int frame_count = 0;

    g723_init_decoder();
    G723DecoderContext* decoder_ctx = g723_create_context();
    if (!decoder_ctx) return;

    FILE* log = fopen("d:/Projects/C++/UDF/test/udf_debug.log", "a");
    if (!log) {
        log = fopen("d:/Projects/C++/UDF/test/udf_debug2.log", "a");
    }
    if (log) {
        fprintf(log, "=== START transcode fixed ===\n");
        fclose(log);
    }

    bool eof_reached = false;
    unsigned int chunk_size_limit = static_cast<unsigned int>(in_blob->blob_max_segment);
    if (chunk_size_limit == 0 || chunk_size_limit > 65535) chunk_size_limit = 65535;

    while (true) {
        // Наполнение входного буфера
        while (!eof_reached) {
            unsigned int space_left = max_seg_size - buffer_data_size;
            if (space_left < chunk_size_limit) break; // Буфер полон, сначала обработаем

            unsigned short bytes_read = 0;
            unsigned short max_read = static_cast<unsigned short>(chunk_size_limit);
            
            // Вызов InterBase API
            short result = in_blob->blob_get_segment(in_blob->blob_handle,
                reinterpret_cast<char*>(input_buffer.data() + buffer_data_size),
                max_read, &bytes_read);

            if (bytes_read > 0) {
                buffer_data_size += bytes_read;
            }

            // ИСПРАВЛЕНИЕ: result == 1 означает "последний сегмент прочитан", 
            // но bytes_read при этом содержал валидные данные! 
            // result == 343597383 или другие коды — ошибки/конец потока.
            if (result != 0 && bytes_read == 0) { 
                eof_reached = true; 
                break; 
            }
            if (result != 0 && result != 1) {
                eof_reached = true; // Защита от критических ошибок чтения страниц
                break;
            }
        }

        // КРИТИЧЕСКОЕ ИСПРАВЛЕНИЕ: Удален преждевременный break, 
        // ломавший логику, если весь файл поместился в один буфер.
        // Парсим фреймы до тех пор, пока буфер содержит хотя бы минимальный кадр SID
        size_t input_pos = 0;
        while (input_pos + G723_FRAME_SIZE_SID <= buffer_data_size) {
            int bytes_left = buffer_data_size - static_cast<int>(input_pos);
            if (bytes_left < G723_FRAME_SIZE_SID) break;

            unsigned char first_byte = input_buffer[input_pos];
            int current_frame_size = 0;
            
            // Побитовый анализ типа кадра G.723.1
            switch (first_byte & 0x03) {
                case 0x00: current_frame_size = G723_MAX_FRAME_SIZE; break; // 6.3 kbps (24 bytes)
                case 0x01: current_frame_size = 20; break;                  // 5.3 kbps (20 bytes)
                case 0x02: current_frame_size = G723_FRAME_SIZE_SID; break; // Сверхкомпрессия SID (4 bytes)
                default:   current_frame_size = 1; break;
            }
            
            if (bytes_left < current_frame_size) break; // Пакет оборван, ждем дозагрузки

            int consumed = g723_decode_frame(decoder_ctx, &input_buffer[input_pos], pcm_output);
            if (consumed <= 0) { 
                input_pos += 1; 
                continue; 
            }
            input_pos += consumed;
            frame_count++;

            // Компандирование PCM (Linear) в телефонию PCMU (G.711u)
            encode_pcm_to_pcmu(pcm_output, &output_buffer[output_idx], G723_SAMPLES_PER_FRAME);
            output_idx += G723_SAMPLES_PER_FRAME;
            
            // Если выходной скользящий буфер близок к лимиту unsigned short (65535), сбрасываем его в СУБД
            if (output_idx >= 65535 - G723_SAMPLES_PER_FRAME) {
                flush_output_buffer(out_blob, output_buffer, output_idx);
            }
        }

        // Смещение необработанного остатка данных (фрагмента кадра) в начало буфера
        if (input_pos > 0 && input_pos < buffer_data_size) {
            std::copy(input_buffer.begin() + input_pos, input_buffer.begin() + buffer_data_size, input_buffer.begin());
            buffer_data_size -= static_cast<unsigned int>(input_pos);
        } else if (input_pos >= buffer_data_size) {
            buffer_data_size = 0;
        }

        // Условие гарантированного выхода: сеть/БД пусты и буфер полностью вычитан
        if (eof_reached && buffer_data_size == 0) {
            break;
        }
        
        // Дополнительный барьер: если мы застряли на поврежденном участке
        if (eof_reached && input_pos == 0 && buffer_data_size > 0) {
            break; // Защита от бесконечного цикла на битых байтах в конце BLOB
        }
    }

    // Финальный выталкивающий флаш остатков аудиопотока
    flush_output_buffer(out_blob, output_buffer, output_idx);

    log = fopen("d:/Projects/C++/UDF/test/udf_debug.log", "a");
    if (!log) {
        log = fopen("d:/Projects/C++/UDF/test/udf_debug2.log", "a");
    }
    if (log) { 
        fprintf(log, "FINAL FIXED: frames=%d, output_idx=%zu\n", frame_count, output_idx); 
        fclose(log); 
    }

    g723_destroy_context(decoder_ctx);
}

extern "C" __declspec(dllexport) void __cdecl transcode_g723(BLOB_CB in_blob, BLOB_CB out_blob) {
    if (!g_decoder_initialized || !validate_blob_callbacks(in_blob, out_blob)) return;
    transcode_internal(in_blob, out_blob);
}

extern "C" __declspec(dllexport) void __cdecl transcode_g723_ib_util(BLOB_CB in_blob, BLOB_CB out_blob, void* (*ib_util_malloc)(size_t), void (*ib_util_free)(void*)) {
    (void)ib_util_malloc; (void)ib_util_free;
    if (!g_decoder_initialized || !validate_blob_callbacks(in_blob, out_blob)) return;
    transcode_internal(in_blob, out_blob);
}