#include <windows.h>
#include <cstring>
#include <vector>
#include <new>
#include <algorithm>
#include <cstdio>

#include "g723_1_decoder.h"
#include "g711u_coder.h"

#define G723_SAMPLES_PER_FRAME 240
#define G723_FRAME_SIZE_SID 4
#define G723_MAX_FRAME_SIZE 24
#define G723_MIN_FRAME_SIZE 1

#define MAX_SEG_SIZE 200000

// Жесткая Borland-упаковка для InterBase 2009
#pragma pack(push, 2)
typedef struct blob_callback {
    short   (__stdcall *blob_get_segment) (void*, char*, unsigned short, short*);
    void*   blob_handle;
    
    // cppcheck-suppress unusedStructMember
    unsigned int blob_max_segment;
    
    void    (__stdcall *blob_put_segment) (void*, const char*, unsigned short);
} *BLOB_CB;
// Возвращаем стандартную упаковку для остального кода
#pragma pack(pop)

static bool g_decoder_initialized = false;

// Аппаратно выровненные статические буферы для защиты от SSE/AVX Alignment Fault
__declspec(align(16)) static unsigned char g_input_buffer[MAX_SEG_SIZE];
__declspec(align(16)) static unsigned char g_output_buffer[MAX_SEG_SIZE];
__declspec(align(16)) static double g_pcm_output[G723_SAMPLES_PER_FRAME];

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    (void)hModule; (void)lpReserved;

    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        OutputDebugStringA("[UDF] DllMain: DLL_PROCESS_ATTACH\n");
        g711u_init_encoder();
        g_decoder_initialized = true;
    }
    else if (ul_reason_for_call == DLL_PROCESS_DETACH) {
        OutputDebugStringA("[UDF] DllMain: DLL_PROCESS_DETACH\n");
        g_decoder_initialized = false;
    }
    return TRUE;
}

static inline void encode_pcm_to_pcmu(const double* pcm_input, unsigned char* output, int sample_count) {
    for (int i = 0; i < sample_count; ++i) {
        short sample_short = static_cast<short>(pcm_input[i]);
        output[i] = g711u_linear_to_pcmu(sample_short);
    }
}

static inline void flush_output_buffer(BLOB_CB out_blob, unsigned char* output_buffer, size_t& output_idx) {
    if (output_idx > 0) {
        out_blob->blob_put_segment(out_blob->blob_handle, 
                                   reinterpret_cast<const char*>(output_buffer), 
                                   static_cast<unsigned short>(output_idx));
        output_idx = 0;
    }
}

static void transcode_internal(BLOB_CB in_blob, BLOB_CB out_blob) {
    OutputDebugStringA("[UDF] ВНУТРИ transcode_internal - СТАРТ КОНВЕЙЕРА\n");

    // Обнуляем глобальные статические буферы перед использованием для безопасности
    std::memset(g_input_buffer, 0, MAX_SEG_SIZE);
    std::memset(g_output_buffer, 0, MAX_SEG_SIZE);
    std::memset(g_pcm_output, 0, sizeof(g_pcm_output));

    // ====================================================================
    // ЖЕСТКОЕ ИСПРАВЛЕНИЕ: ТОЛЬКО ЧИСТЫЕ СИ-ТИПЫ! НИКАКИХ std::vector!
    // ====================================================================
    unsigned int buffer_data_size = 0;
    size_t output_idx = 0;
    int frame_count = 0;

    // Создаем контекст декодера
    G723DecoderContext* decoder_ctx = g723_create_context();
    if (!decoder_ctx) {
        OutputDebugStringA("[UDF] КРИТИЧЕСКАЯ ОШИБКА: Не удалось создать контекст декодера g723\n");
        return;
    }

    short bytes_read = 0;
    short result = 0;
    
    // Прямолинейный цикл чтения сегментов
    do {
        unsigned int space_left = MAX_SEG_SIZE - buffer_data_size;
        if (space_left < 4096) break; 

        bytes_read = 0;
        result = in_blob->blob_get_segment(
            in_blob->blob_handle,
            reinterpret_cast<char*>(g_input_buffer + buffer_data_size),
            4096, 
            &bytes_read
        );

        if (bytes_read > 0) {
            buffer_data_size += bytes_read;
        }

    } while (result == 0);

    // Лог после гарантированного вычитывания всех страниц
    char dbg_loop[128]; // ИСПРАВЛЕНО: строго массив символов вместо char
    sprintf_s(dbg_loop, sizeof(dbg_loop), "[UDF] Начинаем парсинг: buffer_data_size=%u\n", buffer_data_size);
    OutputDebugStringA(dbg_loop);
    
    // Если база пуста, сразу выходим БЕЗ деструкторов и БЕЗ падений кучи
    if (buffer_data_size == 0) {
        OutputDebugStringA("[UDF] ВНИМАНИЕ: Входной BLOB пуст, транскодирование отменено\n");
        g723_destroy_context(decoder_ctx);
        return; // <--- ТЕПЕРЬ ЭТОТ ВЫХОД АБСОЛЮТНО БЕЗОПАСЕН, КОНФЛИКТА КУЧИ НЕТ!
    }
    
    size_t input_pos = 0;
    while (input_pos + G723_FRAME_SIZE_SID <= buffer_data_size) {
        size_t bytes_left_calc = buffer_data_size - input_pos;
        if (bytes_left_calc < G723_FRAME_SIZE_SID) break;

        char dbg_step[128]; // ИСПРАВЛЕНО: строго массив символов вместо char
        sprintf_s(dbg_step, sizeof(dbg_step), "[UDF] Итерация парсера: pos=%zu, left=%lu\n", input_pos, (unsigned long)bytes_left_calc);
        OutputDebugStringA(dbg_step);

        unsigned char first_byte = g_input_buffer[input_pos];
        int current_frame_size = 0;
        
        switch (first_byte & 0x03) {
            case 0x00: current_frame_size = G723_MAX_FRAME_SIZE; break; 
            case 0x01: current_frame_size = 20; break;                  
            case 0x02: current_frame_size = G723_FRAME_SIZE_SID; break; 
            default:   current_frame_size = 1; break;
        }
        
        if (bytes_left_calc < static_cast<size_t>(current_frame_size)) break;

        char dbg_frame[128]; // ИСПРАВЛЕНО: строго массив символов вместо char
        sprintf_s(dbg_frame, sizeof(dbg_frame), "[UDF] Вызов g723_decode_frame: pos=%zu, size=%d\n", input_pos, current_frame_size);
        OutputDebugStringA(dbg_frame);

        int consumed = g723_decode_frame(decoder_ctx, &g_input_buffer[input_pos], g_pcm_output);
        if (consumed <= 0) { 
            input_pos += 1; 
            continue; 
        }
        input_pos += consumed;
        frame_count++;

        encode_pcm_to_pcmu(g_pcm_output, &g_output_buffer[output_idx], G723_SAMPLES_PER_FRAME);
        output_idx += G723_SAMPLES_PER_FRAME;
        
        if (output_idx >= 65535 - G723_SAMPLES_PER_FRAME) {
            flush_output_buffer(out_blob, g_output_buffer, output_idx);
        }
    }

    flush_output_buffer(out_blob, g_output_buffer, output_idx);

    char dbg_final[128]; // ИСПРАВЛЕНО: строго массив символов вместо char
    sprintf_s(dbg_final, sizeof(dbg_final), "[UDF] УСПЕШНЫЙ ФИНАЛ: Обработано кадров=%d, байт=%zu\n", frame_count, output_idx);
    OutputDebugStringA(dbg_final);

    g723_destroy_context(decoder_ctx);
}

extern "C" void __stdcall transcode_g723(BLOB_CB in_blob, BLOB_CB out_blob) {
    if (!g_decoder_initialized || !in_blob || !out_blob) return;
    transcode_internal(in_blob, out_blob);
}

extern "C" void __stdcall transcode_g723_ib_util(BLOB_CB in_blob, BLOB_CB out_blob, void* (*ib_util_malloc)(size_t), void (*ib_util_free)(void*)) {
    (void)ib_util_malloc; (void)ib_util_free;
    if (!g_decoder_initialized || !in_blob || !out_blob) return;
    transcode_internal(in_blob, out_blob);
}
