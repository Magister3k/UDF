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

namespace g723 {

struct Decoder::Impl {
    DecoderConfig config;
    bool use_pf = true;

    Impl(const DecoderConfig& cfg) : config(cfg) {
        use_pf = cfg.use_pf;

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

struct CodecProcessor::Impl {
    std::unique_ptr<Decoder> decoder;
    ProcessingStats stats;
};

CodecProcessor::CodecProcessor(const DecoderConfig& dec_config)
    : pimpl_(std::make_unique<Impl>()) {
    pimpl_->decoder = std::make_unique<Decoder>(dec_config);
}

CodecProcessor::~CodecProcessor() = default;

Result<ProcessingStats> CodecProcessor::process_file(
    const std::string& input_path,
    const std::string& output_path,
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
    return process_stream(input, output, callback);
}

Result<ProcessingStats> CodecProcessor::process_stream(
    std::istream& input,
    std::ostream& output,
    ProgressCallback callback
) {
    pimpl_->stats = ProcessingStats{};

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
        auto result = pimpl_->decoder->decode(frame_obj);
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

    return Result<ProcessingStats>(pimpl_->stats);
}

} // namespace g723