#include <windows.h>
#include <cstring>
#include <vector>
#include <new>
#include <algorithm>
#include "g723_1_decoder.h"
#include "g711u_coder.h"

#ifdef _DEBUG
#define DEBUG_OUTPUT(msg) OutputDebugStringA(msg)
#else
#define DEBUG_OUTPUT(msg)
#endif

#define G723_SAMPLES_PER_FRAME 240
#define G723_FRAME_SIZE_SID 4
#define G723_MAX_FRAME_SIZE 24
#define G723_MIN_FRAME_SIZE 1

typedef struct blob_callback {
    short   (*blob_get_segment) (void*, char*, unsigned short, unsigned short*);
    void*   blob_handle;
    long    blob_number_segments;  // cppcheck-suppress unusedStructMember
    long    blob_max_segment;
    long    blob_total_length;     // cppcheck-suppress unusedStructMember
    void    (*blob_put_segment) (void*, const char*, unsigned short);
} *BLOB_CB;

static bool g_decoder_initialized = false;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    (void)hModule;
    (void)lpReserved;

    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        if (!g711u_init_encoder()) {
            DEBUG_OUTPUT("Failed to initialize G.711 encoder\n");
            return FALSE;
        }
        if (!g723_init_decoder()) {
            DEBUG_OUTPUT("Failed to initialize G.723 decoder\n");
            return FALSE;
        }
        g_decoder_initialized = true;
    }
    else if (ul_reason_for_call == DLL_PROCESS_DETACH) {
        g723_cleanup_decoder();
        g_decoder_initialized = false;
    }
    return TRUE;
}

static inline bool validate_blob_callbacks(const BLOB_CB in_blob, const BLOB_CB out_blob) {
    if (!in_blob || !out_blob) {
        DEBUG_OUTPUT("Null BLOB callback pointer\n");
        return false;
    }
    if (!in_blob->blob_handle || !in_blob->blob_get_segment) {
        DEBUG_OUTPUT("Invalid input BLOB handle or get_segment\n");
        return false;
    }
    if (!out_blob->blob_put_segment) {
        DEBUG_OUTPUT("Invalid output BLOB put_segment\n");
        return false;
    }
    if (in_blob->blob_max_segment <= 0 || in_blob->blob_max_segment > 65535) {
        DEBUG_OUTPUT("Invalid max segment size\n");
        return false;
    }
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
    const unsigned short max_seg_size = 32768;

    std::vector<unsigned char> input_buffer(max_seg_size);
    std::vector<unsigned char> output_buffer(max_seg_size);
    double pcm_output[G723_SAMPLES_PER_FRAME];

    int buffer_data_size = 0;
    size_t output_idx = 0;

    G723DecoderContext* decoder_ctx = g723_create_context();
    if (!decoder_ctx) {
        DEBUG_OUTPUT("Failed to create decoder context\n");
        return;
    }

    DEBUG_OUTPUT("Starting G.723 to PCMU transcoding\n");

    while (true) {
        unsigned short bytes_read = 0;
        short result = in_blob->blob_get_segment(in_blob->blob_handle, reinterpret_cast<char*>(input_buffer.data() + buffer_data_size), max_seg_size - buffer_data_size, &bytes_read);

        if (result != 0) {
            break;
        }

        if (bytes_read == 0) {
            break;
        }

        buffer_data_size += bytes_read;

        size_t input_pos = 0;

        while (input_pos + G723_FRAME_SIZE_SID <= static_cast<size_t>(buffer_data_size)) {
            int bytes_left = buffer_data_size - static_cast<int>(input_pos);

            if (bytes_left < G723_FRAME_SIZE_SID) {
                break;
            }

            unsigned char first_byte = input_buffer[input_pos];
            int current_frame_size = 0;
            int crnt_crate = 0;

            switch (first_byte & 0x03) {
                case 0x00: current_frame_size = G723_MAX_FRAME_SIZE; crnt_crate = 0; break;
                case 0x01: current_frame_size = 20; crnt_crate = 1; break;
                case 0x02: current_frame_size = G723_FRAME_SIZE_SID; crnt_crate = 2; break;
                default:   current_frame_size = 1; crnt_crate = 3; break;
            }

            if (bytes_left < current_frame_size) {
                break;
            }

            int consumed = g723_decode_frame(decoder_ctx, &input_buffer[input_pos], pcm_output);
            if (consumed <= 0) {
                input_pos += 1;
                continue;
            }

            input_pos += consumed;

            encode_pcm_to_pcmu(pcm_output, &output_buffer[output_idx], G723_SAMPLES_PER_FRAME);
            output_idx += G723_SAMPLES_PER_FRAME;

            if (output_idx >= max_seg_size) {
                flush_output_buffer(out_blob, output_buffer, output_idx);
            }
        }

        if (input_pos > 0 && input_pos < static_cast<size_t>(buffer_data_size)) {
            std::copy(input_buffer.begin() + input_pos, input_buffer.begin() + buffer_data_size, input_buffer.begin());
            buffer_data_size -= static_cast<int>(input_pos);
        } else if (input_pos >= static_cast<size_t>(buffer_data_size)) {
            buffer_data_size = 0;
        }
    }

    flush_output_buffer(out_blob, output_buffer, output_idx);
    g723_destroy_context(decoder_ctx);
    DEBUG_OUTPUT("Transcoding completed\n");
}

extern "C" __declspec(dllexport) void __cdecl transcode_g723(BLOB_CB in_blob, BLOB_CB out_blob) {
    if (!g_decoder_initialized || !validate_blob_callbacks(in_blob, out_blob)) {
        return;
    }
    transcode_internal(in_blob, out_blob);
}

extern "C" __declspec(dllexport) void __cdecl transcode_g723_ib_util(BLOB_CB in_blob, BLOB_CB out_blob, void* (*ib_util_malloc)(size_t), void (*ib_util_free)(void*)) {
    (void)ib_util_malloc;
    (void)ib_util_free;

    if (!g_decoder_initialized || !validate_blob_callbacks(in_blob, out_blob)) {
        return;
    }
    transcode_internal(in_blob, out_blob);
}