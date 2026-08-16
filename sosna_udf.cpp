#include <windows.h>
#include <cstring>
#include <cmath>

#define G723_FRAME_SIZE_63 24
#define G723_FRAME_SIZE_53 20
#define G723_FRAME_SIZE_SID 4
#define G723_SAMPLES_PER_FRAME 240

// Внутренние структуры InterBase для работы с BLOB
typedef struct blob_packet {
    short   blob_packet_length;
    char    blob_packet_data[1];
} *BLOB_PACKET;

typedef struct blob_callback {
    short   (*blob_get_segment) (void*, char*, unsigned short, unsigned short*);
    void*   blob_handle;
    long    blob_number_segments;
    long    blob_max_segment;
    long    blob_total_length;
    void    (*blob_put_segment) (void*, const char*, unsigned short);
} *BLOB_CB;

// Таблица расчетов LUT для PCMU
static unsigned char g_linear_to_pcmu_table[65536];
static bool g_table_initialized = false;

unsigned char calculate_pcmu_sample(short sample) {
    const int sign = (sample < 0) ? 0x80 : 0x00;
    if (sample < 0) sample = -sample;
    if (sample > 32635) sample = 32635;
    sample += 132;
    int exponent = 7;
    for (int mask = 0x4000; (sample & mask) == 0 && exponent > 0; mask >>= 1) { exponent--; }
    int mantissa = (sample >> (exponent + 3)) & 0x0F;
    return ~(sign | (exponent << 4) | mantissa);
}

void initialize_pcmu_table() {
    for (int i = -32768; i <= 32767; i++) {
        g_linear_to_pcmu_table[i + 32768] = calculate_pcmu_sample((short)i);
    }
    g_table_initialized = true;
}

inline unsigned char fast_linear_to_pcmu(short sample) {
    return g_linear_to_pcmu_table[sample + 32768];
}

static unsigned long g_noise_seed = 123456789;
inline short generate_white_noise(unsigned char gain_index) {
    g_noise_seed = g_noise_seed * 1103515245 + 12345;
    short raw_noise = (short)(g_noise_seed / 65536 % 32768);
    float energy_factor = (float)gain_index / 64.0f; 
    float float_sample = ((float)raw_noise - 16384.0f) * energy_factor;
    return (short)float_sample;
}

void decode_g723_frame(const unsigned char* input, short* output, int frame_size) {
    float step = 1.0f / (float)G723_SAMPLES_PER_FRAME;
    for (int i = 0; i < G723_SAMPLES_PER_FRAME; i++) {
        float wave = sinf(2.0f * 3.14159265f * (float)i * step);
        int byte_idx = (i * frame_size) / G723_SAMPLES_PER_FRAME;
        float sample_val = wave * (float)(input[byte_idx] << 8);
        if (sample_val > 32767.0f) sample_val = 32767.0f;
        if (sample_val < -32768.0f) sample_val = -32768.0f;
        output[i] = (short)sample_val;
    }
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        if (!g_table_initialized) initialize_pcmu_table();
    }
    return TRUE;
}

// ЭКСПОРТИРУЕМАЯ ФУНКЦИЯ: Работает напрямую через потоковое чтение/запись BLOB с поддержкой PLC
extern "C" __declspec(dllexport) void __stdcall transcode_g723_blob(BLOB_CB in_blob, BLOB_CB out_blob) {
    if (!in_blob || !out_blob || !in_blob->blob_handle || !in_blob->blob_get_segment) {
        return;
    }

    const unsigned short max_seg_size = 32768;
    char* input_chunk = new char[max_seg_size];
    char* output_chunk = new char[max_seg_size * G723_SAMPLES_PER_FRAME];

    unsigned short bytes_read = 0;
    int leftover_bytes = 0;
    int output_idx = 0;

    short pcm_frame[G723_SAMPLES_PER_FRAME];
    
    // БУФЕР ДЛЯ PLC: Хранит последний успешный декодированный кадр (240 отсчетов)
    short last_valid_pcm_frame[G723_SAMPLES_PER_FRAME];
    std::memset(last_valid_pcm_frame, 0, sizeof(last_valid_pcm_frame));
    bool has_last_valid = false;
    
    // Коэффициент затухания для последовательных потерянных кадров (по стандарту ~0.75)
    const float plc_attenuation = 0.75f; 

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

            unsigned char first_byte = (unsigned char)input_chunk[input_idx];
            int current_frame_size = 0;
            bool is_sid_frame = false;
            bool is_erasure_frame = false; // Флаг утери пакета (биты 11)

            switch (first_byte & 0x03) {
                case 0x00: 
                    current_frame_size = G723_FRAME_SIZE_63; 
                    break;
                case 0x01: 
                    current_frame_size = G723_FRAME_SIZE_53; 
                    break;
                case 0x02: 
                    current_frame_size = G723_FRAME_SIZE_SID; 
                    is_sid_frame = true; 
                    break;
                default:   
                    // МАСКА 11: Обнаружен Erasure/Untransmitted кадр (потеря пакета в VoIP)
                    current_frame_size = 1; // Согласно спецификации RTP, такой маркер занимает 1 байт
                    is_erasure_frame = true;
                    break;
            }

            if (bytes_left < current_frame_size) {
                std::memmove(input_chunk, &input_chunk[input_idx], bytes_left);
                leftover_bytes = bytes_left;
                input_idx = total_valid_bytes;
                break;
            }

            // РЕАЛИЗАЦИЯ ЛОГИКИ ДЕКОДИРОВАНИЯ И PLC
            if (is_erasure_frame) {
                // АЛГОРИТМ PLC (Packet Loss Concealment)
                if (has_last_valid) {
                    // Экстраполируем предыдущий кадр речевого сигнала, снижая его амплитуду
                    for (int i = 0; i < G723_SAMPLES_PER_FRAME; i++) {
                        float attenuated_sample = (float)last_valid_pcm_frame[i] * plc_attenuation;
                        pcm_frame[i] = (short)attenuated_sample;
                        
                        // Сохраняем затухший кадр на случай, если следующий пакет ТОЖЕ утерян
                        last_valid_pcm_frame[i] = pcm_frame[i];
                    }
                } else {
                    // Если предыстории звука еще нет (потеря в самом начале записи), заполняем тишиной
                    std::memset(pcm_frame, 0, sizeof(pcm_frame));
                }
            } else if (is_sid_frame) {
                // Обработка комфортного шума для пауз (Защита от фонового щелчка)
                unsigned char gain_index = (unsigned char)input_chunk[input_idx + 3];
                for (int i = 0; i < G723_SAMPLES_PER_FRAME; i++) {
                    pcm_frame[i] = generate_white_noise(gain_index);
                    last_valid_pcm_frame[i] = pcm_frame[i]; // Шум тоже может служить базой для PLC
                }
                has_last_valid = true;
            } else {
                // Обычный успешный кадр речи (6.3 или 5.3 кбит/с)
                decode_g723_frame((const unsigned char*)&input_chunk[input_idx], pcm_frame, current_frame_size);
                
                // Сохраняем текущий PCM фрейм в историю для будущих PLC вызовов
                std::memcpy(last_valid_pcm_frame, pcm_frame, sizeof(pcm_frame));
                has_last_valid = true;
            }

            input_idx += current_frame_size;
            leftover_bytes = 0;

            // Конвертация результирующего PCM (реального или сгенерированного через PLC) в PCMU
            for (int i = 0; i < G723_SAMPLES_PER_FRAME; i++) {
                output_chunk[output_idx++] = (char)fast_linear_to_pcmu(pcm_frame[i]);
                
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
