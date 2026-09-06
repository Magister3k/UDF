#include "g723_codec.hpp"
#include "g723_types.hpp"
#include <cstring>
#include <cmath>
#include <chrono>
#include <fstream>
#include <vector>

extern "C" {
#include "typedef2.h"
#include "cst2.h"
#include "coder2.h"
#include "decod2.h"
#include "codcng2.h"
#include "deccng2.h"
#include "vad2.h"
#include "util2.h"
#include "lpc2.h"
#include "lsp2.h"
#include "exc2.h"
#include "tab2.h"
#include "lbccode2.h"
#include "utilcng2.h"
}

namespace g723 {

struct Encoder::Impl {
    EncoderConfig config;
    Word16 work_rate = Rate63;
    bool use_hp = true;
    bool use_vad = true;
    bool use_pf = true;

    Impl(const EncoderConfig& cfg) : config(cfg) {
        work_rate = (cfg.rate == CodecRate::Rate63) ? Rate63 : Rate53;
        use_hp = cfg.use_hp;
        use_vad = cfg.use_vad;
        use_pf = cfg.use_pf;
        
        // Set global flags
        ::UseHp = use_hp ? True : False;
        ::UseVx = use_vad ? True : False;
        ::UsePf = use_pf ? True : False;
        
        Init_Coder();
        Init_Cod_Cng();
        Init_Vad();
    }

    Result<BitstreamFrame> encode_frame(std::span<const Float> samples) {
        if (samples.size() < 240) {
            return Result<BitstreamFrame>("Insufficient samples for encoding (need 240)");
        }

        FLOAT data_buff[Frame];
        for (int i = 0; i < Frame; ++i) {
            data_buff[i] = samples[i];
        }

        char vout[24] = {0};
        Coder(data_buff, vout);

        BitstreamFrame result;
        result.data.assign(vout, vout + 24);
        
        Word16 info = vout[0] & 0x03;
        switch (info) {
            case 0: result.type = FrameType::Active; result.rate = CodecRate::Rate63; break;
            case 1: result.type = FrameType::Active; result.rate = CodecRate::Rate53; break;
            case 2: result.type = FrameType::SID; break;
            case 3: result.type = FrameType::Untransmitted; break;
        }
        
        int size = 0;
        if (result.type == FrameType::Active) {
            size = (result.rate == CodecRate::Rate63) ? 24 : 20;
        } else if (result.type == FrameType::SID) {
            size = 4;
        } else {
            size = 1;
        }
        result.data.resize(size);
        
        return Result<BitstreamFrame>(std::move(result));
    }
};

Encoder::Encoder(const EncoderConfig& config) : pimpl_(std::make_unique<Impl>(config)) {}
Encoder::~Encoder() = default;
Encoder::Encoder(Encoder&&) noexcept = default;
Encoder& Encoder::operator=(Encoder&&) noexcept = default;

Result<BitstreamFrame> Encoder::encode(std::span<const Float> samples) {
    return pimpl_->encode_frame(samples);
}

Result<BitstreamFrame> Encoder::encode(const AudioFrame& frame) {
    return encode(std::span(frame.samples.data(), frame.valid_samples));
}

void Encoder::reset() {
    Init_Coder();
    Init_Cod_Cng();
    Init_Vad();
}

EncoderState Encoder::get_state() const {
    EncoderState state;
    std::copy_n(CodStat.PrevLsp, 10, state.prev_lsp.begin());
    std::copy_n(CodStat.PrevWgt, 145, state.prev_wgt.begin());
    std::copy_n(CodStat.PrevErr, 145, state.prev_err.begin());
    std::copy_n(CodStat.PrevExc, 145, state.prev_exc.begin());
    std::copy_n(CodStat.PrevDat, 150, state.prev_dat.begin());
    std::copy_n(CodStat.WghtFirDl, 10, state.wght_fir_dl.begin());
    std::copy_n(CodStat.WghtIirDl, 10, state.wght_iir_dl.begin());
    std::copy_n(CodStat.RingFirDl, 10, state.ring_fir_dl.begin());
    std::copy_n(CodStat.RingIirDl, 10, state.ring_iir_dl.begin());
    state.sin_det = CodStat.SinDet;
    std::copy_n(CodStat.Err, 5, state.err.begin());
    state.hpf_zdl = CodStat.HpfZdl;
    state.hpf_pdl = CodStat.HpfPdl;
    return state;
}

void Encoder::set_state(const EncoderState& state) {
    std::copy_n(state.prev_lsp.begin(), 10, CodStat.PrevLsp);
    std::copy_n(state.prev_wgt.begin(), 145, CodStat.PrevWgt);
    std::copy_n(state.prev_err.begin(), 145, CodStat.PrevErr);
    std::copy_n(state.prev_exc.begin(), 145, CodStat.PrevExc);
    std::copy_n(state.prev_dat.begin(), 150, CodStat.PrevDat);
    std::copy_n(state.wght_fir_dl.begin(), 10, CodStat.WghtFirDl);
    std::copy_n(state.wght_iir_dl.begin(), 10, CodStat.WghtIirDl);
    std::copy_n(state.ring_fir_dl.begin(), 10, CodStat.RingFirDl);
    std::copy_n(state.ring_iir_dl.begin(), 10, CodStat.RingIirDl);
    CodStat.SinDet = state.sin_det;
    std::copy_n(state.err.begin(), 5, CodStat.Err);
    CodStat.HpfZdl = state.hpf_zdl;
    CodStat.HpfPdl = state.hpf_pdl;
}

CodecRate Encoder::get_rate() const { return pimpl_->config.rate; }
void Encoder::set_rate(CodecRate rate) { 
    pimpl_->config.rate = rate; 
    pimpl_->work_rate = (rate == CodecRate::Rate63) ? Rate63 : Rate53;
}

WorkMode Encoder::get_mode() const { return pimpl_->config.mode; }
void Encoder::set_mode(WorkMode mode) { pimpl_->config.mode = mode; }

struct Decoder::Impl {
    DecoderConfig config;
    bool use_pf = true;

    Impl(const DecoderConfig& cfg) : config(cfg) {
        use_pf = cfg.use_pf;
        
        // Set global flags
        ::UsePf = use_pf ? True : False;
        
        Init_Decod();
        Init_Dec_Cng();
    }

    Result<AudioFrame> decode_frame(const BitstreamFrame& frame) {
        FLOAT data_buff[Frame] = {0};
        char vinp[24] = {0};
        
        size_t copy_size = std::min(frame.data.size(), size_t(24));
        std::copy_n(frame.data.data(), copy_size, vinp);
        
        Word16 crc = frame.crc_error ? 1 : 0;
        Decod(data_buff, vinp, crc);

        AudioFrame result;
        for (int i = 0; i < Frame; ++i) {
            result.samples[i] = data_buff[i];
        }
        result.valid_samples = Frame;
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

DecoderState Decoder::get_state() const {
    DecoderState state;
    std::copy_n(DecStat.PrevLsp, 10, state.prev_lsp.begin());
    std::copy_n(DecStat.PrevExc, 145, state.prev_exc.begin());
    std::copy_n(DecStat.SyntIirDl, 10, state.synt_iir_dl.begin());
    std::copy_n(DecStat.PostFirDl, 10, state.post_fir_dl.begin());
    std::copy_n(DecStat.PostIirDl, 10, state.post_iir_dl.begin());
    state.ecount = DecStat.Ecount;
    state.inter_gain = DecStat.InterGain;
    state.inter_indx = DecStat.InterIndx;
    state.rseed = DecStat.Rseed;
    state.park = DecStat.Park;
    state.gain = DecStat.Gain;
    return state;
}

void Decoder::set_state(const DecoderState& state) {
    std::copy_n(state.prev_lsp.begin(), 10, DecStat.PrevLsp);
    std::copy_n(state.prev_exc.begin(), 145, DecStat.PrevExc);
    std::copy_n(state.synt_iir_dl.begin(), 10, DecStat.SyntIirDl);
    std::copy_n(state.post_fir_dl.begin(), 10, DecStat.PostFirDl);
    std::copy_n(state.post_iir_dl.begin(), 10, DecStat.PostIirDl);
    DecStat.Ecount = state.ecount;
    DecStat.InterGain = state.inter_gain;
    DecStat.InterIndx = state.inter_indx;
    DecStat.Rseed = state.rseed;
    DecStat.Park = state.park;
    DecStat.Gain = state.gain;
}

bool Decoder::get_use_pf() const { return pimpl_->use_pf; }
void Decoder::set_use_pf(bool use_pf) { pimpl_->use_pf = use_pf; }

Codec::Codec(const EncoderConfig& enc_config, const DecoderConfig& dec_config) 
    : encoder_(std::make_unique<Encoder>(enc_config))
    , decoder_(std::make_unique<Decoder>(dec_config)) {}

Codec::~Codec() = default;
Codec::Codec(Codec&&) noexcept = default;
Codec& Codec::operator=(Codec&&) noexcept = default;

Result<BitstreamFrame> Codec::encode(std::span<const Float> samples) {
    return encoder_->encode(samples);
}

Result<AudioFrame> Codec::decode(const BitstreamFrame& frame) {
    return decoder_->decode(frame);
}

void Codec::reset() {
    encoder_->reset();
    decoder_->reset();
}

Encoder& Codec::encoder() { return *encoder_; }
Decoder& Codec::decoder() { return *decoder_; }
const Encoder& Codec::encoder() const { return *encoder_; }
const Decoder& Codec::decoder() const { return *decoder_; }

struct CodecProcessor::Impl {
    std::unique_ptr<Codec> codec;
    ProcessingStats stats;
};

CodecProcessor::CodecProcessor(const EncoderConfig& enc_config, const DecoderConfig& dec_config)
    : pimpl_(std::make_unique<Impl>()) {
    pimpl_->codec = std::make_unique<Codec>(enc_config, dec_config);
}

CodecProcessor::~CodecProcessor() = default;

Result<ProcessingStats> CodecProcessor::process_file(
    const std::string& input_path,
    const std::string& output_path,
    bool encode,
    ProgressCallback callback
) {
    std::ifstream input(input_path, std::ios::binary);
    if (!input) {
        return Result<ProcessingStats>("Failed to open input file: " + input_path);
    }
    std::ofstream output(output_path, std::ios::binary);
    if (!output) {
        return Result<ProcessingStats>("Failed to open output file: " + output_path);
    }
    return process_stream(input, output, encode, callback);
}

Result<ProcessingStats> CodecProcessor::process_stream(
    std::istream& input,
    std::ostream& output,
    bool encode,
    ProgressCallback callback
) {
    pimpl_->stats = ProcessingStats{};

    if (encode) {
        std::vector<int16_t> pcm_buffer(Frame);
        while (input.read(reinterpret_cast<char*>(pcm_buffer.data()), Frame * sizeof(int16_t))) {
            std::vector<Float> float_buffer(Frame);
            for (int i = 0; i < Frame; ++i) {
                float_buffer[i] = static_cast<Float>(pcm_buffer[i]);
            }
            
            auto frame_start = std::chrono::high_resolution_clock::now();
            auto result = pimpl_->codec->encode(float_buffer);
            auto frame_end = std::chrono::high_resolution_clock::now();
            
            if (!result) {
                return Result<ProcessingStats>("Encode error: " + result.error());
            }
            
            output.write(reinterpret_cast<const char*>(result.value().data.data()), result.value().data.size());
            
            pimpl_->stats.frames_processed++;
            pimpl_->stats.bytes_encoded += result.value().data.size();
            pimpl_->stats.total_encode_time_ms += 
                std::chrono::duration<double, std::milli>(frame_end - frame_start).count();
            
            if (callback) {
                callback(0.0f, pimpl_->stats);
            }
        }
    } else {
        uint8_t header;
        while (input.read(reinterpret_cast<char*>(&header), 1)) {
            int info = header & 0x03;
            FrameType type;
            CodecRate rate;
            int frame_size;
            
            switch (info) {
                case 0: type = FrameType::Active; rate = CodecRate::Rate63; frame_size = 24; break;
                case 1: type = FrameType::Active; rate = CodecRate::Rate53; frame_size = 20; break;
                case 2: type = FrameType::SID; rate = CodecRate::Rate63; frame_size = 4; break;
                default: type = FrameType::Untransmitted; rate = CodecRate::Rate63; frame_size = 1; break;
            }
            
            std::vector<uint8_t> frame_data(frame_size);
            frame_data[0] = header;
            if (frame_size > 1) {
                input.read(reinterpret_cast<char*>(frame_data.data() + 1), frame_size - 1);
            }
            
            BitstreamFrame frame_obj;
            frame_obj.data = std::move(frame_data);
            frame_obj.type = type;
            frame_obj.rate = rate;
            
            auto frame_start = std::chrono::high_resolution_clock::now();
            auto result = pimpl_->codec->decode(frame_obj);
            auto frame_end = std::chrono::high_resolution_clock::now();
            
            if (!result) {
                return Result<ProcessingStats>("Decode error: " + result.error());
            }
            
            std::vector<int16_t> pcm_out(Frame);
            for (int i = 0; i < Frame; ++i) {
                Float val = result.value().samples[i];
                if (val < -32767.5) val = -32768.0;
                else if (val > 32766.5) val = 32767.0;
                pcm_out[i] = (val >= 0) ? static_cast<int16_t>(val + 0.5) : static_cast<int16_t>(val - 0.5);
            }
            
            output.write(reinterpret_cast<const char*>(pcm_out.data()), Frame * sizeof(int16_t));
            
            pimpl_->stats.frames_processed++;
            pimpl_->stats.bytes_decoded += frame_obj.data.size();
            pimpl_->stats.total_decode_time_ms += 
                std::chrono::duration<double, std::milli>(frame_end - frame_start).count();
            
            if (callback) {
                callback(0.0f, pimpl_->stats);
            }
        }
    }
    
    return Result<ProcessingStats>(pimpl_->stats);
}

} // namespace g723