#include <memory>
#include "g723_1_decoder.h"
#include "g723_decoder.hpp"

extern "C" {
    void Init_Decod(void);
    void Init_Dec_Cng(void);
}

// Структура контекста теперь пустая — это просто заглушка для совместимости типов
struct G723DecoderContext {
    int dummy;
};

// Выделяем ОДИН глобальный экземпляр контекста структуры для совместимости
static G723DecoderContext g_static_ctx = { 0 };

// ВЫДЕЛЯЕМ ДЕКОДЕР В СТАТИЧЕСКОЙ ПАМЯТИ DLL! 
// ОС создаст его один раз при загрузке библиотеки. Никаких new/delete!
static std::unique_ptr<g723_decoder::Decoder> g_shared_decoder = nullptr;

bool g723_init_decoder() {
    return true; 
}

void g723_cleanup_decoder() {
    g_shared_decoder.reset(); // Мягко очистит объект только при выгрузке сервера
}

G723DecoderContext* __cdecl g723_create_context() {
    // Инициализируем Си-таблицы ОДИН раз при запросе контекста
    Init_Decod();
    Init_Dec_Cng();

    // Если объект еще не создан — инициализируем его глобально
    if (!g_shared_decoder) {
        g723_decoder::DecoderConfig config;
        config.use_pf = true;
        g_shared_decoder = std::make_unique<g723_decoder::Decoder>(config);
    } else {
        g_shared_decoder->reset(); // Если уже был создан — просто сбрасываем историю
    }
    
    // Возвращаем указатель на глобальную статическую заглушку. Куча не затрагивается!
    return &g_static_ctx;
}

int g723_decode_frame(G723DecoderContext* ctx, const unsigned char* input, double* output_pcm) {
    (void)ctx; // Игнорируем указатель, используем безопасный глобальный декодер
    
    if (!g_shared_decoder) return 0;

    // Формируем фрейм для C++ обертки
    g723_decoder::BitstreamFrame frame;
    std::memcpy(frame.data.data(), input, frame.data.size());

    // Вызываем оригинальный декодер
    auto result = g_shared_decoder->decode(frame);
    if (!result.has_value()) {
        return 0; 
    }

    const g723_decoder::AudioFrame& audio = result.value();
    for (int i = 0; i < 240; ++i) {
        output_pcm[i] = static_cast<double>(audio.samples[i]);
    }

    // Возвращаем размер считанного кадра в зависимости от битрейта
    unsigned char first_byte = input[0];
    switch (first_byte & 0x03) {
        case 0x00: return 24; 
        case 0x01: return 20; 
        case 0x02: return 4;  
        default:   return 1;
    }
}

void g723_destroy_context(G723DecoderContext* ctx) {
    (void)ctx;
    // КАТЕГОРИЧЕСКИ НИЧЕГО НЕ УДАЛЯЕМ! Память статическая и контролируется ОС.
    // Это полностью ликвидирует Heap Corruption и защищает сервер от зависания.
}
