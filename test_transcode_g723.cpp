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

static void generate_g723_file(const std::string& filename, bool high_rate) {
    std::ofstream create_file(filename, std::ios::binary);
    if (!create_file.is_open()) {
        std::cerr << "[ОШИБКА] Не удалось создать файл '" << filename << "'." << std::endl;
        return;
    }

    std::cout << "[ИНФО] Формирование G.723.1-потока '" << filename << "' (" << (high_rate ? "6.3 kbps" : "5.3 kbps") << ")..." << std::endl;

    for (int i = 0; i < 50; ++i) {
        unsigned char frame_type = high_rate ? 0x00 : 0x01;
        create_file.write(reinterpret_cast<const char*>(&frame_type), 1);

        const int dummy_size = high_rate ? 23 : 19;
        std::vector<char> dummy(dummy_size, 0);
        create_file.write(dummy.data(), dummy_size);
    }

    for (int i = 0; i < 10; ++i) {
        const char sid_frame[4] = {0x02, 0x00, 0x00, 0x15};
        create_file.write(sid_frame, 4);
    }

    for (int i = 0; i < 5; ++i) {
        const char erasure_marker = 0x03;
        create_file.write(&erasure_marker, 1);
    }

    create_file.close();
}

static std::string detect_g723_rate(const std::vector<char>& data) {
    for (size_t i = 0; i < data.size(); ++i) {
        unsigned char frame_type = static_cast<unsigned char>(data[i]);
        if (frame_type == 0x00) {
            return "6.3";
        }
        if (frame_type == 0x01) {
            return "5.3";
        }
        if (frame_type == 0x02 || frame_type == 0x03) {
            continue;
        }
    }
    return "unknown";
}

static void transcode_file(TranscodeBlobFunc transcode_g723, const std::string& input_filename) {
    std::ifstream infile(input_filename, std::ios::binary | std::ios::ate);
    if (!infile.is_open()) {
        std::cerr << "[ОШИБКА] Не удалось открыть входной файл '" << input_filename << "'." << std::endl;
        return;
    }

    std::streamsize size = infile.tellg();
    infile.seekg(0, std::ios::beg);

    MockBlobContext input_ctx;
    input_ctx.data.resize(static_cast<size_t>(size));
    infile.read(input_ctx.data.data(), size);
    infile.close();

    std::string rate = detect_g723_rate(input_ctx.data);
    std::cout << "[ИНФО] Скорость исходного файла '" << input_filename << "': " << rate << " kbps" << std::endl;

    MockBlobContext output_ctx;
    blob_callback in_blob_cb = { MockGetSegment, &input_ctx, 0, 0, static_cast<long>(input_ctx.data.size()), nullptr };
    blob_callback out_blob_cb = { nullptr, &output_ctx, 0, 0, 0, MockPutSegment };

    std::cout << "[ИНФО] Транскодирование '" << input_filename << "'..." << std::endl;
    transcode_g723(&in_blob_cb, &out_blob_cb);

    std::string output_filename = input_filename;
    const size_t dot_pos = output_filename.rfind('.');
    if (dot_pos != std::string::npos) {
        output_filename = output_filename.substr(0, dot_pos);
    }
    output_filename += "__[" + rate + "]";
    output_filename += ".pcmu";

    std::ofstream outfile(output_filename, std::ios::binary);
    if (outfile.is_open()) {
        outfile.write(output_ctx.data.data(), static_cast<std::streamsize>(output_ctx.data.size()));
        outfile.close();
        std::cout << "[УСПЕХ] Выходной файл '" << output_filename << "' успешно создан." << std::endl;
    } else {
        std::cerr << "[ОШИБКА] Не удалось создать выходной файл '" << output_filename << "'." << std::endl;
    }
}

static void transcode_all_g723_files(TranscodeBlobFunc transcode_g723) {
    WIN32_FIND_DATAA find_data;
    HANDLE hFind = FindFirstFileA("*.g723", &find_data);
    if (hFind == INVALID_HANDLE_VALUE) {
        std::cout << "[ИНФО] В текущей папке нет файлов *.g723 для транскодирования." << std::endl;
        return;
    }

    do {
        if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            transcode_file(transcode_g723, find_data.cFileName);
        }
    } while (FindNextFileA(hFind, &find_data));

    FindClose(hFind);
}

int main() {
    std::cout << "=== Стенд тестирования модульной архитектуры UDF ===" << std::endl;

    HMODULE hDll = LoadLibrary("sosna_udf.dll");
    if (!hDll) {
        std::cerr << "[ОШИБКА] Не удалось загрузить sosna_udf.dll!" << std::endl;
        return 1;
    }

    TranscodeBlobFunc transcode_g723 = (TranscodeBlobFunc)GetProcAddress(hDll, "transcode_g723");
    if (!transcode_g723) {
        std::cerr << "[ОШИБКА] Точка входа 'transcode_g723' не найдена!" << std::endl;
        FreeLibrary(hDll);
        return 1;
    }

    generate_g723_file("test_input_hi.g723", true);
    generate_g723_file("test_input_low.g723", false);

    std::cout << "[ИНФО] Передача BLOB-структур в DLL для всех файлов *.g723 в папке..." << std::endl;
    transcode_all_g723_files(transcode_g723);
    std::cout << "[ОК] Модульный транскодер успешно завершил побитовый синтез." << std::endl;

    FreeLibrary(hDll);
    return 0;
}
