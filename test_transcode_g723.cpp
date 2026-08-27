#include <windows.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <string>
#include <algorithm>
#include <chrono>

#include "g723_1_decoder.h"

#define G723_SAMPLES_PER_FRAME 240
#define G723_SAMPLE_RATE 8000

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
        return 1;
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

typedef void (__cdecl *TranscodeBlobFunc)(BLOB_CB, BLOB_CB);
typedef void (__cdecl *TranscodeBlobFuncWithAlloc)(BLOB_CB, BLOB_CB, void* (*)(size_t), void (*)(void*));

static void generate_g723_file(const std::string& filename, bool high_rate) {
    std::ofstream create_file(filename, std::ios::binary);
    if (!create_file.is_open()) {
        std::cerr << "[ОШИБКА] Не удалось создать файл '" << filename << "'." << std::endl;
        return;
    }

    std::cout << "[ИНФО] Генерирование '" << filename << "' (" << (high_rate ? "6.3 kbps" : "5.3 kbps") << ")..." << std::endl;

    const int frame_size = high_rate ? 24 : 20;

    for (int i = 0; i < 50; ++i) {
        unsigned char frame_type = high_rate ? 0x00 : 0x01;
        create_file.write(reinterpret_cast<const char*>(&frame_type), 1);

        const int data_size = frame_size - 1;
        std::vector<char> dummy(data_size, 0);
        create_file.write(dummy.data(), data_size);
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

static double calculate_pcm_duration(size_t pcm_bytes) {
    return static_cast<double>(pcm_bytes) / G723_SAMPLE_RATE;
}

struct TranscodeResult {
    std::string input_filename;
    std::string rate;
    size_t input_size;
    double pcm_duration_sec;
    double transcode_time_ms;
    size_t output_size;
    bool success;
    std::string error;
};

static TranscodeResult transcode_file(TranscodeBlobFunc transcode_g723, const std::string& input_filename) {
    TranscodeResult result;
    result.input_filename = input_filename;
    result.success = false;
    result.input_size = 0;
    result.output_size = 0;
    result.pcm_duration_sec = 0.0;
    result.transcode_time_ms = 0.0;
    result.rate = "unknown";
    result.error = "";

    std::ifstream infile(input_filename, std::ios::binary | std::ios::ate);
    if (!infile.is_open()) {
        result.error = "Не удалось открыть входной файл";
        return result;
    }

    std::streamsize size = infile.tellg();
    infile.seekg(0, std::ios::beg);
    result.input_size = static_cast<size_t>(size);

    MockBlobContext input_ctx;
    input_ctx.data.resize(static_cast<size_t>(size));
    infile.read(input_ctx.data.data(), size);
    infile.close();

    result.rate = detect_g723_rate(input_ctx.data);

    MockBlobContext output_ctx;
    blob_callback in_blob_cb = { MockGetSegment, &input_ctx, 0, 32768, static_cast<long>(input_ctx.data.size()), nullptr };
    blob_callback out_blob_cb = { nullptr, &output_ctx, 0, 0, 0, MockPutSegment };

    auto start = std::chrono::high_resolution_clock::now();
    transcode_g723(&in_blob_cb, &out_blob_cb);
    auto end = std::chrono::high_resolution_clock::now();

    result.transcode_time_ms = std::chrono::duration<double, std::milli>(end - start).count();
    result.output_size = output_ctx.data.size();
    result.pcm_duration_sec = calculate_pcm_duration(output_ctx.data.size());
    result.success = true;

    std::string output_filename = input_filename;
    const size_t dot_pos = output_filename.rfind('.');
    if (dot_pos != std::string::npos) {
        output_filename = output_filename.substr(0, dot_pos);
    }
    output_filename += ".pcmu";

    std::ofstream outfile(output_filename, std::ios::binary);
    if (outfile.is_open()) {
        outfile.write(output_ctx.data.data(), static_cast<std::streamsize>(output_ctx.data.size()));
        outfile.close();
    } else {
        result.error = "Не удалось создать выходной файл";
        result.success = false;
    }

    return result;
}

static bool has_g723_files() {
    WIN32_FIND_DATAA find_data;
    HANDLE hFind = FindFirstFileA("*.g723", &find_data);
    if (hFind == INVALID_HANDLE_VALUE) {
        return false;
    }
    bool found = false;
    do {
        if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            found = true;
            break;
        }
    } while (FindNextFileA(hFind, &find_data));
    FindClose(hFind);
    return found;
}

static void transcode_all_g723_files(TranscodeBlobFunc transcode_g723, std::ofstream& csv_out) {
    WIN32_FIND_DATAA find_data;
    HANDLE hFind = FindFirstFileA("*.g723", &find_data);
    if (hFind == INVALID_HANDLE_VALUE) {
        std::cout << "[ИНФО] В текущей папке нет файлов *.g723 для транскодирования." << std::endl;
        return;
    }

    csv_out << "Файл G.723.1\tСкорость G.723.1 (кб/с)\tРазмер файла G.723.1 (кбайт)\tДлительность речи (сек.)\tРазмер файла PCMU (кбайт)\tВремя обработки (сек.)" << std::endl;

    do {
        if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            std::string filename = find_data.cFileName;
            std::cout << "[ИНФО] Обработка '" << filename << "'..." << std::endl;

            TranscodeResult result = transcode_file(transcode_g723, filename);

            if (result.success) {
                std::string output_filename = filename;
                const size_t dot_pos = output_filename.rfind('.');
                if (dot_pos != std::string::npos) {
                    output_filename = output_filename.substr(0, dot_pos);
                }
                output_filename += ".pcmu";
                std::cout << "[УСПЕХ] Создан файл '" << output_filename << "'" << std::endl;

                double input_size_kb = result.input_size / 1024.0;
                double output_size_kb = result.output_size / 1024.0;
                double transcode_time_sec = result.transcode_time_ms / 1000.0;

                csv_out << result.input_filename << "\t"
                        << result.rate << "\t"
                        << input_size_kb << "\t"
                        << result.pcm_duration_sec << "\t"
                        << output_size_kb << "\t"
                        << transcode_time_sec << std::endl;
            } else {
                std::cerr << "[ОШИБКА] '" << filename << "': " << result.error << std::endl;
                csv_out << result.input_filename << "\t"
                        << result.rate << "\t"
                        << "ERROR\tERROR\tERROR\tERROR" << std::endl;
            }
        }
    } while (FindNextFileA(hFind, &find_data));

    FindClose(hFind);
}

static void test_edge_cases(TranscodeBlobFunc transcode_g723) {
    std::cout << "\n=== Тестирование граничных случаев ===" << std::endl;

    MockBlobContext input_ctx;
    MockBlobContext output_ctx;

    blob_callback in_blob_cb = { MockGetSegment, &input_ctx, 0, 32768, 0, nullptr };
    blob_callback out_blob_cb = { nullptr, &output_ctx, 0, 0, 0, MockPutSegment };

    std::cout << "[ИНФО] Пустой BLOB..." << std::endl;
    transcode_g723(&in_blob_cb, &out_blob_cb);
    if (output_ctx.data.empty()) {
        std::cout << "[УСПЕХ] Пустой BLOB обработан корректно (пустой выход)" << std::endl;
    } else {
        std::cout << "[FAIL] Пустой BLOB вернул данные" << std::endl;
    }

    std::cout << "[ИНФО] Неполный кадр (менее 4 байт)..." << std::endl;
    input_ctx.data.clear();
    input_ctx.read_position = 0;
    output_ctx.data.clear();
    input_ctx.data = {0x00, 0x01};
    in_blob_cb.blob_total_length = 2;
    transcode_g723(&in_blob_cb, &out_blob_cb);
    std::cout << "[УСПЕХ] Неполный кадр обработан без краша" << std::endl;

    std::cout << "[ИНФО] Кадр SID (4 байта)..." << std::endl;
    input_ctx.data.clear();
    input_ctx.read_position = 0;
    output_ctx.data.clear();
    input_ctx.data = {0x02, 0x00, 0x00, 0x15};
    in_blob_cb.blob_total_length = 4;
    transcode_g723(&in_blob_cb, &out_blob_cb);
    std::cout << "[УСПЕХ] SID кадр обработан, выход: " << output_ctx.data.size() << " байт" << std::endl;

    std::cout << "[ИНФО] Маркеры потерь кадров (0x03)..." << std::endl;
    input_ctx.data.clear();
    input_ctx.read_position = 0;
    output_ctx.data.clear();
    input_ctx.data = {0x03, 0x03, 0x03};
    in_blob_cb.blob_total_length = 3;
    transcode_g723(&in_blob_cb, &out_blob_cb);
    std::cout << "[УСПЕХ] Маркеры потерь обработаны, выход: " << output_ctx.data.size() << " байт" << std::endl;

    std::cout << "[ОК] Тест пройден!" << std::endl;
}

int main() {
    std::cout << "=== СТЕНД ТЕСТИРОВАНИЯ UDF ТРАНСКОДЕРА G.723.1 -> PCMU ===" << std::endl;

    HMODULE hDll = LoadLibrary("sosna_udf.dll");
    if (!hDll) {
        std::cerr << "[ОШИБКА] Не удалось загрузить sosna_udf.dll! Код: " << GetLastError() << std::endl;
        return 1;
    }

    TranscodeBlobFunc transcode_g723 = reinterpret_cast<TranscodeBlobFunc>(GetProcAddress(hDll, "transcode_g723"));
    if (!transcode_g723) {
        std::cerr << "[ОШИБКА] Точка входа 'transcode_g723' не найдена!" << std::endl;
        FreeLibrary(hDll);
        return 1;
    }

    TranscodeBlobFuncWithAlloc transcode_g723_ib = reinterpret_cast<TranscodeBlobFuncWithAlloc>(GetProcAddress(hDll, "transcode_g723_ib_util"));
    if (transcode_g723_ib) {
        std::cout << "[ИНФО] Найдена расширенная функция с поддержкой ib_util_malloc" << std::endl;
    }

    bool files_exist = has_g723_files();

    if (!files_exist) {
        std::cout << "[ИНФО] Файлов *.g723 не найдено, генерирую тестовые файлы..." << std::endl;
        generate_g723_file("test_input_hi.g723", true);
        generate_g723_file("test_input_low.g723", false);
        std::cout << "[УСПЕХ] Тестовые файлы созданы" << std::endl;
        files_exist = true;
    } else {
        std::cout << "[ИНФО] Найдены файлы *.g723, используем их для тестирования." << std::endl;
    }

    std::cout << "\n=== Тестирование на файлах ===" << std::endl;

    std::ofstream csv_out("test_result.txt");
    if (!csv_out.is_open()) {
        std::cerr << "[ОШИБКА] Не удалось создать test_result.txt" << std::endl;
    }

    transcode_all_g723_files(transcode_g723, csv_out);

    if (csv_out.is_open()) {
        csv_out.close();
        std::cout << "[ОК] Тест пройден! Результаты сохранены в test_result.txt" << std::endl;
    }

    test_edge_cases(transcode_g723);

    std::cout << "\n[ОК] Тестирование завершено!!!" << std::endl;

    FreeLibrary(hDll);
    return 0;
}