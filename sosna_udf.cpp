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
    // blob_max_segment может быть 0 в некоторых версиях InterBase — допускаем
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

    // Явная инициализация глобальных таблиц декодера перед созданием контекста
    g723_init_decoder();
    G723DecoderContext* decoder_ctx = g723_create_context();
    if (!decoder_ctx) return;

    // InterBase API: blob_get_segment возвращает 0 — есть ещё данные, 1 — последний сегмент прочитан
    // Читаем сегменты пока не получим result != 0 (конец BLOB)
    bool eof_reached = false;
    unsigned int chunk_size_limit = static_cast<unsigned int>(in_blob->blob_max_segment);
    if (chunk_size_limit == 0 || chunk_size_limit > 65535) chunk_size_limit = 65535;

    while (true) {
        // Читаем жадно: заполняем буфер целиком пока есть место и данные.
        // result == 0: данные есть, продолжаем читать
        // result != 0: последний сегмент прочитан (или ошибка), больше не читаем
        while (!eof_reached) {
            unsigned int space_left = max_seg_size - buffer_data_size;
            if (space_left < chunk_size_limit) break;  // буфер почти полон — обработаем сначала
            unsigned short bytes_read = 0;
            unsigned short max_read = static_cast<unsigned short>(chunk_size_limit);
            short result = in_blob->blob_get_segment(in_blob->blob_handle,
                reinterpret_cast<char*>(input_buffer.data() + buffer_data_size),
                max_read, &bytes_read);

            if (bytes_read > 0) buffer_data_size += bytes_read;
            if (result != 0) { eof_reached = true; break; }  // конец BLOB
        }

        if (buffer_data_size < G723_FRAME_SIZE_SID) {
            if (eof_reached && buffer_data_size > 0) {
                size_t input_pos = 0;
                while (input_pos + G723_FRAME_SIZE_SID <= buffer_data_size) {
                    int bytes_left = buffer_data_size - input_pos;
                    if (bytes_left < G723_FRAME_SIZE_SID) break;
                    unsigned char first_byte = input_buffer[input_pos];
                    int current_frame_size = 0;
                    switch (first_byte & 0x03) {
                        case 0x00: current_frame_size = G723_MAX_FRAME_SIZE; break;
                        case 0x01: current_frame_size = 20; break;
                        case 0x02: current_frame_size = G723_FRAME_SIZE_SID; break;
                        default:   current_frame_size = 1; break;
                    }
                    if (bytes_left < current_frame_size) break;
                    int consumed = g723_decode_frame(decoder_ctx, &input_buffer[input_pos], pcm_output);
                    if (consumed <= 0) { input_pos += 1; continue; }
                    input_pos += consumed;
                    frame_count++;
                    encode_pcm_to_pcmu(pcm_output, &output_buffer[output_idx], G723_SAMPLES_PER_FRAME);
                    output_idx += G723_SAMPLES_PER_FRAME;
                    if (output_idx >= 65535 - G723_SAMPLES_PER_FRAME) flush_output_buffer(out_blob, output_buffer, output_idx);
                }
            }
            break;
        }

        size_t input_pos = 0;
        while (input_pos + G723_FRAME_SIZE_SID <= buffer_data_size) {
            int bytes_left = buffer_data_size - static_cast<int>(input_pos);
            if (bytes_left < G723_FRAME_SIZE_SID) break;

            unsigned char first_byte = input_buffer[input_pos];
            int current_frame_size = 0;
            switch (first_byte & 0x03) {
                case 0x00: current_frame_size = G723_MAX_FRAME_SIZE; break;
                case 0x01: current_frame_size = 20; break;
                case 0x02: current_frame_size = G723_FRAME_SIZE_SID; break;
                default:   current_frame_size = 1; break;
            }
            if (bytes_left < current_frame_size) break;

            int consumed = g723_decode_frame(decoder_ctx, &input_buffer[input_pos], pcm_output);
            if (consumed <= 0) { input_pos += 1; continue; }
            input_pos += consumed;
            frame_count++;

            encode_pcm_to_pcmu(pcm_output, &output_buffer[output_idx], G723_SAMPLES_PER_FRAME);
            output_idx += G723_SAMPLES_PER_FRAME;
            // InterBase BLOB API: max segment size = 65535 (unsigned short).
            // Флашим заранее, чтобы не превысить лимит при приведении к unsigned short.
            if (output_idx >= 65535 - G723_SAMPLES_PER_FRAME) flush_output_buffer(out_blob, output_buffer, output_idx);
        }

        // Копируем необработанный хвост в начало буфера
        if (input_pos > 0 && input_pos < buffer_data_size) {
            std::copy(input_buffer.begin() + input_pos, input_buffer.begin() + buffer_data_size, input_buffer.begin());
            buffer_data_size -= static_cast<unsigned int>(input_pos);
        } else if (input_pos >= buffer_data_size) {
            buffer_data_size = 0;
        }

        if (eof_reached && buffer_data_size == 0) break;
    }

    flush_output_buffer(out_blob, output_buffer, output_idx);
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