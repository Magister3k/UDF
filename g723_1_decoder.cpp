#include <memory>
#include <cstring>
#include "g723_1_decoder.h"
#include "g723_decoder.hpp"

extern "C" {
    void Init_Decod(void);
    void Init_Dec_Cng(void);
}

// ��������� ��������� ������ ������ � ��� ������ �������� ��� ������������� �����
struct G723DecoderContext {
    int dummy;
};

// �������� ���� ���������� ��������� ��������� ��������� ��� �������������
static G723DecoderContext g_static_ctx = { 0 };

// �������� ������� � ����������� ������ DLL! 
// �� ������� ��� ���� ��� ��� �������� ����������. ������� new/delete!
static std::unique_ptr<g723_decoder::Decoder> g_shared_decoder = nullptr;

bool g723_init_decoder() {
    return true; 
}

void g723_cleanup_decoder() {
    g_shared_decoder.reset(); // ����� ������� ������ ������ ��� �������� �������
}

G723DecoderContext* __cdecl g723_create_context() {
    // �������������� ��-������� ���� ��� ��� ������� ���������
    Init_Decod();
    Init_Dec_Cng();

    // ���� ������ ��� �� ������ � �������������� ��� ���������
    if (!g_shared_decoder) {
        g723_decoder::DecoderConfig config;
        config.use_pf = true;
        g_shared_decoder = std::make_unique<g723_decoder::Decoder>(config);
    } else {
        g_shared_decoder->reset(); // ���� ��� ��� ������ � ������ ���������� �������
    }
    
    // ���������� ��������� �� ���������� ����������� ��������. ���� �� �������������!
    return &g_static_ctx;
}

int g723_decode_frame(G723DecoderContext* ctx, const unsigned char* input, double* output_pcm) {
    (void)ctx; // ���������� ���������, ���������� ���������� ���������� �������
    
    if (!g_shared_decoder) return 0;

    // ��������� ����� ��� C++ �������
    g723_decoder::BitstreamFrame frame;
    frame.data.resize(24);
    const size_t frame_size = (input[0] & 0x03) == 0x01 ? 20u :
                              (input[0] & 0x03) == 0x02 ? 4u :
                              (input[0] & 0x03) == 0x03 ? 1u : 24u;
    std::memset(frame.data.data(), 0, frame.data.size());
    std::memcpy(frame.data.data(), input, frame_size);

    // �������� ������������ �������
    auto result = g_shared_decoder->decode(frame);
    if (!result.has_value()) {
        return 0; 
    }

    const g723_decoder::AudioFrame& audio = result.value();
    for (int i = 0; i < 240; ++i) {
        output_pcm[i] = static_cast<double>(audio.samples[i]);
    }

    // ���������� ������ ���������� ����� � ����������� �� ��������
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
    // ������������� ������ �� �������! ������ ����������� � �������������� ��.
    // ��� ��������� ����������� Heap Corruption � �������� ������ �� ���������.
}
