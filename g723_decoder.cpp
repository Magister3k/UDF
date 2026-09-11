#include "g723_decoder.hpp"
#include <algorithm>

extern "C" {
    #include "typedef2.h"
    #include "cst2.h"
    #include "decod2.h"
    #include "deccng2.h"
    #include "util2.h"
    #include "lpc2.h"
    #include "lsp2.h"
    #include "exc2.h"
    #include "tab2.h"
    #include "lbccode2.h"
    #include "utilcng2.h"
}

namespace g723_decoder {

struct Decoder::Impl {
    DecoderConfig config;
    bool use_pf = true;

    Impl(const DecoderConfig& cfg) : config(cfg) {
        use_pf = cfg.use_pf;
    }

    Result<AudioFrame> decode_frame(std::span<const uint8_t> data, bool crc_error) {
        FLOAT data_buff[240] = {0};
        char vinp[24] = {0};
        
        size_t copy_size = std::min(data.size(), size_t(24));
        std::copy_n(data.data(), copy_size, vinp);
        
        Word16 crc = crc_error ? 1 : 0;
        Decod(data_buff, vinp, crc);

        AudioFrame result;
        for (int i = 0; i < 240; ++i) {
            result.samples[i] = data_buff[i];
        }
        result.valid_samples = 240;
        return Result<AudioFrame>(std::move(result));
    }
};

Decoder::Decoder(const DecoderConfig& config) : pimpl_(std::make_unique<Impl>(config)) {}
Decoder::~Decoder() = default;
Decoder::Decoder(Decoder&&) noexcept = default;
Decoder& Decoder::operator=(Decoder&&) noexcept = default;

Result<AudioFrame> Decoder::decode(const BitstreamFrame& frame) {
    return pimpl_->decode_frame(frame.data, frame.crc_error);
}

Result<AudioFrame> Decoder::decode(std::span<const uint8_t> data, FrameType type, CodecRate rate, bool crc) {
    (void)type;
    (void)rate;
    return pimpl_->decode_frame(data, crc);
}

void Decoder::reset() {
    Init_Decod();
    Init_Dec_Cng();
}

bool Decoder::get_use_pf() const { return pimpl_->use_pf; }
void Decoder::set_use_pf(bool use_pf) { 
    pimpl_->use_pf = use_pf;
    ::UsePf = use_pf ? True : False;
}

} // namespace g723_decoder