#include "g711u_coder.h"

// Статическая таблица расчетов на 65536 значений (скрыта внутри этого файла)
static unsigned char g_linear_to_pcmu_table[65536];
static bool g_table_initialized = false;

// Внутренняя историческая функция компрессии
static unsigned char calculate_pcmu_sample(short sample) {
    const int sign = (sample < 0) ? 0x80 : 0x00;
    if (sample < 0) sample = -sample;
    if (sample > 32635) sample = 32635;
    sample += 132;
    
    int exponent = 7;
    for (int mask = 0x4000; (sample & mask) == 0 && exponent > 0; mask >>= 1) {
        exponent--;
    }
    int mantissa = (sample >> (exponent + 3)) & 0x0F;
    return ~(sign | (exponent << 4) | mantissa);
}

// Заполнение таблицы в оперативной памяти (O(N) при старте)
void g711u_init_encoder() {
    if (g_table_initialized) return;
    
    for (int i = -32768; i <= 32767; i++) {
        g_linear_to_pcmu_table[i + 32768] = calculate_pcmu_sample((short)i);
    }
    g_table_initialized = true;
}

// Мгновенное чтение из памяти по индексу (O(1) при работе)
unsigned char g711u_linear_to_pcmu(short sample) {
    return g_linear_to_pcmu_table[sample + 32768];
}
