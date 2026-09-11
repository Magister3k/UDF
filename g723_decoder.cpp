#include "g723_decoder.hpp"
#include <cstring>
#include <cmath>
#include <vector>
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
        //::UsePf = use_pf ? True : False;
        //Init_Decod();
        //Init_Dec_Cng();
    }

    Result<AudioFrame> decode_frame(const BitstreamFrame& frame) {
        FLOAT data_buff[240] = {0};
        char vinp[24] = {0};
        
        size_t copy_size = std::min(frame.data.size(), size_t(24));
        std::copy_n(frame.data.data(), copy_size, vinp);
        
        Word16 crc = frame.crc_error ? 1 : 0;
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
    return pimpl_->decode_frame(frame);
}

Result<AudioFrame> Decoder::decode(std::span<const uint8_t> data, FrameType type, CodecRate rate, bool crc) {
    BitstreamFrame frame;
    frame.data.assign(data.begin(), data.end());
    frame.type = type;
    frame.rate = rate;
    frame.crc_error = crc;
    return decode(frame);
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