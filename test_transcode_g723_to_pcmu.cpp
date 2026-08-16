#include <windows.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>

// Нам больше не нужно подключать сложные TYPEDEF2.H и DECOD2.H сюда!
// Тест работает через тот же чистый заголовочный файл интерфейса
#include "g723_1_decoder.h" 

#define G723_SAMPLES_PER_FRAME 240

// Эмуляция структур BLOB-коллбэков СУБД InterBase 2009
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

short MockGetSegment(void* handle, char* buffer, unsigned short max_length, unsigned short* bytes_read) {
    MockBlobContext* ctx = static_cast<MockBlobContext*>(handle);
    if (ctx->read_position >= ctx->data.size()) {
        *bytes_read = 0;
        return 1; // EOF
    }
    size_t available = ctx->data.size() - ctx->read_position;
    size_t to_read = (available < max_length) ? available : max_length;
    std::memcpy(buffer, &ctx->data[ctx->read_position], to_read);
    ctx->read_position += to_read;
    *bytes_read = static_cast<unsigned short>(to_read);
    return 0;
}

void MockPutSegment(void* handle, const char* buffer, unsigned short length) {
    MockBlobContext* ctx = static_cast<MockBlobContext*>(handle);
    ctx->data.insert(ctx->data.end(), buffer, buffer + length);
}

typedef void (__stdcall *TranscodeBlobFunc)(BLOB_CB, BLOB_CB);

int main() {
    std::cout << "=== Стенд тестирования модульной архитектуры UDF ===" << std::endl;

    // 1. Загрузка нашей DLL
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

    // 2. Генерация тестового потока VoIP-данных с сетевыми аномалиями (Речь -> SID -> PLC)
    std::string input_filename = "test_input.g723";
    std::ofstream create_file(input_filename, std::ios::binary);
    if (create_file.is_open()) {
        std::cout << "[ИНФО] Формирование тестового VoIP-потока..." << std::endl;
        // Речь 6.3 и 5.3
        for (int i = 0; i < 50; i++) {
            char frame = (i % 2 == 0) ? 0x00 : 0x01;
            create_file.write(&frame, 1);
            char dummy[23] = {0};
            create_file.write(dummy, (i % 2 == 0) ? 23 : 19);
        }
        // Пауза (SID)
        for (int i = 0; i < 10; i++) {
            char sid_frame[4] = {0x02, 0x00, 0x00, 0x15};
            create_file.write(sid_frame, 4);
        }
        // Сетевые потери (Erasure для PLC)
        for (int i = 0; i < 5; i++) {
            char erasure_marker = 0x03;
            create_file.write(&erasure_marker, 1);
        }
        create_file.close();
    }

    // 3. Чтение файла в эмулятор BLOB
    std::ifstream infile(input_filename, std::ios::binary | std::ios::ate);
    std::streamsize size = infile.tellg();
    infile.seekg(0, std::ios::beg);
    MockBlobContext input_ctx;
    input_ctx.data.resize(size);
    infile.read(input_ctx.data.data(), size);
    infile.close();

    // 4. Подготовка контекстов InterBase
    MockBlobContext output_ctx;
    blob_callback in_blob_cb = { MockGetSegment, &input_ctx, 0, 0, static_cast<long>(input_ctx.data.size()), nullptr };
    blob_callback out_blob_cb = { nullptr, &output_ctx, 0, 0, 0, MockPutSegment };

    // 5. Вызов UDF из DLL
    std::cout << "[ИНФО] Передача BLOB-структур в DLL..." << std::endl;
    transcode_g723_blob(&in_blob_cb, &out_blob_cb);
    std::cout << "[ОК] Модульный транскодер успешно завершил побитовый синтез." << std::endl;

    // 6. Сохранение итогового файла
    std::string output_filename = "test_output.pcmu";
    std::ofstream outfile(output_filename, std::ios::binary);
    if (outfile.is_open()) {
        outfile.write(output_ctx.data.data(), output_ctx.data.size());
        outfile.close();
        std::cout << "[УСПЕХ] Выходной файл '" << output_filename << "' успешно создан." << std::endl;
    }

    FreeLibrary(hDll);
    return 0;
}
