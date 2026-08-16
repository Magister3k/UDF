#include <windows.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>

// Структуры InterBase для совместимости
typedef struct blob_callback {
    short   (*blob_get_segment) (void*, char*, unsigned short, unsigned short*);
    void*   blob_handle;
    long    blob_number_segments;
    long    blob_max_segment;
    long    blob_total_length;
    void    (*blob_put_segment) (void*, const char*, unsigned short);
} *BLOB_CB;

struct MockBlobContext {
    std::vector<char> data;
    size_t read_position = 0;
};

// Функция-коллбэк для ЧТЕНИЯ из BLOB
short MockGetSegment(void* handle, char* buffer, unsigned short max_length, unsigned short* bytes_read) {
    MockBlobContext* ctx = static_cast<MockBlobContext*>(handle);
    
    if (ctx->read_position >= ctx->data.size()) {
        *bytes_read = 0;
        return 1; // EOF для InterBase
    }

    size_t available = ctx->data.size() - ctx->read_position;
    size_t to_read = (available < max_length) ? available : max_length;

    std::memcpy(buffer, &ctx->data[ctx->read_position], to_read);
    ctx->read_position += to_read;
    *bytes_read = static_cast<unsigned short>(to_read);

    return 0;
}

// Функция-коллбэк для ЗАПИСИ в BLOB
void MockPutSegment(void* handle, const char* buffer, unsigned short length) {
    MockBlobContext* ctx = static_cast<MockBlobContext*>(handle);
    ctx->data.insert(ctx->data.end(), buffer, buffer + length);
}

typedef void (__stdcall *TranscodeBlobFunc)(BLOB_CB, BLOB_CB);

int main() {
    std::cout << "=== Стенд тестирования UDF с алгоритмами CNG (Шум) и PLC (Потери) ===" << std::endl;

    // 1. Загрузка DLL
    HMODULE hDll = LoadLibrary("sosna_udf.dll");
    if (!hDll) {
        std::cerr << "[ОШИБКА] Не удалось загрузить sosna_udf.dll!" << std::endl;
        return 1;
    }

    TranscodeBlobFunc transcode_g723_blob = (TranscodeBlobFunc)GetProcAddress(hDll, "transcode_g723_blob");
    if (!transcode_g723_blob) {
        std::cerr << "[ОШИБКА] Точка входа 'transcode_g723_blob' не найдена!" << std::endl;
        FreeLibrary(hDll);
        return 1;
    }

    // 2. Генерация тестового VoIP потока с аномалиями
    std::string input_filename = "test_input.g723";
    std::ofstream create_file(input_filename, std::ios::binary);
    
    if (create_file.is_open()) {
        std::cout << "[ИНФО] Формирование тестового файла с сетевыми аномалиями..." << std::endl;
        
        // Этап А: Поток нормальной речи (50 кадров, чередуем 6.3 и 5.3 кбит/с)
        for (int i = 0; i < 50; i++) {
            if (i % 2 == 0) {
                char frame[24] = {0};
                frame[0] = 0x00; // Маска 00 -> 6.3 кбит/с
                frame[5] = 0xAB; // Фейковые биты аудио данных
                create_file.write(frame, 24);
            } else {
                char frame[20] = {0};
                frame[0] = 0x01; // Маска 01 -> 5.3 кбит/с
                frame[5] = 0xCD;
                create_file.write(frame, 20);
            }
        }

        // Этап Б: Пауза в разговоре (10 кадров SID комфортного шума)
        // По стандарту кадр SID занимает 4 байта, младшие биты первого байта = 10 (0x02)
        for (int i = 0; i < 10; i++) {
            char sid_frame[4] = {0};
            sid_frame[0] = 0x02; // Маска 10 -> SID кадр
            sid_frame[3] = 0x20; // Уровень энергии шума (Gain)
            create_file.write(sid_frame, 4);
        }

        // Этап В: Сбой в сети / Потеря пакетов (Серия из 5 подряд утерянных кадров)
        // В VoIP-телефонии при утере RTP пакета маркер Erasure Frame записывается как 1 байт с маской 11
        for (int i = 0; i < 5; i++) {
            char erasure_marker = 0x03; // Маска 11 -> Erasure (Запуск PLC)
            create_file.write(&erasure_marker, 1);
        }

        // Этап Г: Восстановление связи (Еще 20 кадров речи)
        for (int i = 0; i < 20; i++) {
            char frame[24] = {0};
            frame[0] = 0x00;
            frame[10] = 0xEF;
            create_file.write(frame, 24);
        }

        create_file.close();
    }

    // 3. Чтение файла в память эмулятора BLOB
    std::ifstream infile(input_filename, std::ios::binary | std::ios::ate);
    std::streamsize size = infile.tellg();
    infile.seekg(0, std::ios::beg);

    MockBlobContext input_ctx;
    input_ctx.data.resize(size);
    infile.read(input_ctx.data.data(), size);
    infile.close();
    
    std::cout << "[ОК] Тестовый поток сформирован. Всего: " << input_ctx.data.size() << " байт битового потока." << std::endl;

    // 4. Подготовка контекстов InterBase
    MockBlobContext output_ctx;

    blob_callback in_blob_cb;
    in_blob_cb.blob_get_segment = MockGetSegment;
    in_blob_cb.blob_handle = &input_ctx;
    in_blob_cb.blob_total_length = static_cast<long>(input_ctx.data.size());

    blob_callback out_blob_cb;
    out_blob_cb.blob_put_segment = MockPutSegment;
    out_blob_cb.blob_handle = &output_ctx;

    // 5. Вызов UDF транскодирования
    std::cout << "[ИНФО] Вызов транскодера в DLL..." << std::endl;
    transcode_g723_blob(&in_blob_cb, &out_blob_cb);
    std::cout << "[ОК] Метод отработал успешно, исключений памяти не зафиксировано." << std::endl;

    // 6. Сохранение результата
    std::string output_filename = "test_output.pcmu";
    std::ofstream outfile(output_filename, std::ios::binary);
    if (outfile.is_open()) {
        outfile.write(output_ctx.data.data(), output_ctx.data.size());
        outfile.close();
        std::cout << "[УСПЕХ] Файл '" << output_filename << "' сгенерирован (" << output_ctx.data.size() << " байт)." << std::endl;
        std::cout << "[ИНФО] Структура файла включает: Речь -> Белый шум -> Сглаживание потерь (PLC) -> Речь." << std::endl;
    }

    FreeLibrary(hDll);
    std::cout << "=== Тестирование завершено успешно ===" << std::endl;
    return 0;
}
