#pragma once

#include <cstdint>
#include <variant>
#include <vector>
#include <memory>
#include <array>
#include <span>
#include <optional>
#include <string>
#include <expected>

namespace g723 {

using Word16 = int16_t;
using Word32 = int32_t;
using Flag = int;
using Float = double;

constexpr bool False = false;
constexpr bool True = true;

enum class WorkMode : uint8_t {
    Both = 0,
    Encoder = 1,
    Decoder = 2
};

enum class CodecRate : uint8_t {
    Rate63 = 0,
    Rate53 = 1
};

enum class FrameType : uint8_t {
    Untransmitted = 0,
    Active = 1,
    SID = 2
};

struct EncoderConfig {
    WorkMode mode = WorkMode::Both;
    CodecRate rate = CodecRate::Rate63;
    bool use_hp = true;
    bool use_vad = true;
    bool use_pf = true;
};

struct DecoderConfig {
    WorkMode mode = WorkMode::Both;
    bool use_pf = true;
};

struct AudioFrame {
    std::array<Float, 240> samples;
    size_t valid_samples = 240;
};

struct BitstreamFrame {
    std::vector<uint8_t> data;
    FrameType type = FrameType::Untransmitted;
    CodecRate rate = CodecRate::Rate63;
    bool crc_error = false;
};

struct EncoderState {
    std::array<Float, 10> prev_lsp{};
    std::array<Float, 145> prev_wgt{};
    std::array<Float, 145> prev_err{};
    std::array<Float, 145> prev_exc{};
    std::array<Float, 150> prev_dat{};
    std::array<Float, 10> wght_fir_dl{};
    std::array<Float, 10> wght_iir_dl{};
    std::array<Float, 10> ring_fir_dl{};
    std::array<Float, 10> ring_iir_dl{};
    int16_t sin_det = 0;
    std::array<Float, 5> err{};
    Float hpf_zdl = 0.0;
    Float hpf_pdl = 0.0;
};

struct DecoderState {
    std::array<Float, 10> prev_lsp{};
    std::array<Float, 145> prev_exc{};
    std::array<Float, 10> synt_iir_dl{};
    std::array<Float, 10> post_fir_dl{};
    std::array<Float, 10> post_iir_dl{};
    int ecount = 0;
    Float inter_gain = 1.0;
    int16_t inter_indx = 0;
    int16_t rseed = 12345;
    Float park = 0.0;
    Float gain = 1.0;
};

struct CNGEncoderState {
    Float cur_gain = 0.0;
    int16_t past_ftyp = 1;
    std::array<Float, 33> acf{};
    std::array<Float, 10> lsp_sid{};
    std::array<Float, 10> sid_lpc{};
    std::array<Float, 11> rc{};
    std::array<Float, 3> ener{};
    int16_t nb_ener = 0;
    int16_t iref = 0;
    Float sid_gain = 0.0;
    int16_t rand_seed = 12345;
};

struct CNGDecoderState {
    Float cur_gain = 0.0;
    int16_t past_ftyp = 1;
    std::array<Float, 10> lsp_sid{};
    Float sid_gain = 0.0;
    int16_t rand_seed = 12345;
};

struct VADState {
    int16_t hcnt = 3;
    int16_t vcnt = 0;
    Float penr = 1024.0;
    Float nlev = 1024.0;
    int16_t aen = 0;
    std::array<int16_t, 4> polp{1, 1, 60, 60};
    std::array<Float, 10> nlpc{};
};

struct SubframeParams {
    int ac_lg = 0;
    int ac_gn = 0;
    int mamp = 0;
    int grid = 0;
    int tran = 0;
    int pamp = 0;
    int32_t ppos = 0;
};

struct FrameParams {
    int16_t crc = 0;
    int32_t lsp_id = 0;
    std::array<int, 2> olp{};
    std::array<SubframeParams, 4> sfs{};
};

struct PWParams {
    int indx = 0;
    Float gain = 0.0;
};

struct PFParams {
    int indx = 0;
    Float gain = 0.0;
    Float sc_gn = 1.0;
};

using DynamicValue = std::variant<
    bool,
    int8_t, int16_t, int32_t, int64_t,
    uint8_t, uint16_t, uint32_t, uint64_t,
    float, double,
    std::string,
    std::vector<uint8_t>,
    AudioFrame,
    BitstreamFrame,
    EncoderConfig,
    DecoderConfig,
    EncoderState,
    DecoderState,
    CNGEncoderState,
    CNGDecoderState,
    VADState,
    FrameParams,
    SubframeParams,
    PWParams,
    PFParams
>;

template<typename T>
concept AudioSample = std::is_same_v<T, float> || std::is_same_v<T, double> || std::is_same_v<T, int16_t>;

template<AudioSample T>
using AudioBuffer = std::vector<T>;

struct CodecCapabilities {
    static constexpr int frame_size = 240;
    static constexpr int subframes = 4;
    static constexpr int subframe_size = 60;
    static constexpr int lpc_order = 10;
    static constexpr int max_bitrate = 6300;
    static constexpr int min_bitrate = 5300;
    static constexpr int sample_rate = 8000;
};

template<typename T>
class Result {
public:
    Result() = default;
    Result(T&& value) : value_(std::move(value)), has_value_(true) {}
    Result(const T& value) : value_(value), has_value_(true) {}
    Result(std::string error) : error_(std::move(error)), has_value_(false) {}
    
    explicit operator bool() const { return has_value_; }
    bool has_value() const { return has_value_; }
    const T& value() const& { return value_; }
    T& value() & { return value_; }
    const T&& value() const&& { return std::move(value_); }
    T&& value() && { return std::move(value_); }
    const std::string& error() const { return error_; }
    
    T value_or(T&& default_value) const { 
        return has_value_ ? value_ : std::move(default_value); 
    }
    
    const T* operator->() const { return has_value_ ? &value_ : nullptr; }
    T* operator->() { return has_value_ ? &value_ : nullptr; }
    const T& operator*() const& { return value_; }
    T& operator*() & { return value_; }
    const T&& operator*() const&& { return std::move(value_); }
    T&& operator*() && { return std::move(value_); }
    
private:
    T value_;
    std::string error_;
    bool has_value_ = false;
};

} // namespace g723