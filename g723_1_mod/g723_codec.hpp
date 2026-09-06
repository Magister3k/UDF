#pragma once

#include "g723_types.hpp"
#include <memory>
#include <functional>

namespace g723 {

class Encoder {
public:
    explicit Encoder(const EncoderConfig& config = {});
    ~Encoder();

    Encoder(const Encoder&) = delete;
    Encoder& operator=(const Encoder&) = delete;
    Encoder(Encoder&&) noexcept;
    Encoder& operator=(Encoder&&) noexcept;

    [[nodiscard]] Result<BitstreamFrame> encode(std::span<const Float> samples);
    [[nodiscard]] Result<BitstreamFrame> encode(const AudioFrame& frame);
    
    void reset();
    [[nodiscard]] EncoderState get_state() const;
    void set_state(const EncoderState& state);
    
    [[nodiscard]] CodecRate get_rate() const;
    void set_rate(CodecRate rate);
    
    [[nodiscard]] WorkMode get_mode() const;
    void set_mode(WorkMode mode);

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
};

class Decoder {
public:
    explicit Decoder(const DecoderConfig& config = {});
    ~Decoder();

    Decoder(const Decoder&) = delete;
    Decoder& operator=(const Decoder&) = delete;
    Decoder(Decoder&&) noexcept;
    Decoder& operator=(Decoder&&) noexcept;

    [[nodiscard]] Result<AudioFrame> decode(const BitstreamFrame& frame);
    [[nodiscard]] Result<AudioFrame> decode(std::span<const uint8_t> data, FrameType type, CodecRate rate, bool crc = false);
    
    void reset();
    [[nodiscard]] DecoderState get_state() const;
    void set_state(const DecoderState& state);
    
    [[nodiscard]] bool get_use_pf() const;
    void set_use_pf(bool use_pf);

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
};

class Codec {
public:
    Codec(const EncoderConfig& enc_config = {}, const DecoderConfig& dec_config = {});
    ~Codec();

    Codec(const Codec&) = delete;
    Codec& operator=(const Codec&) = delete;
    Codec(Codec&&) noexcept;
    Codec& operator=(Codec&&) noexcept;

    [[nodiscard]] Result<BitstreamFrame> encode(std::span<const Float> samples);
    [[nodiscard]] Result<AudioFrame> decode(const BitstreamFrame& frame);
    
    void reset();
    [[nodiscard]] Encoder& encoder();
    [[nodiscard]] Decoder& decoder();
    [[nodiscard]] const Encoder& encoder() const;
    [[nodiscard]] const Decoder& decoder() const;

private:
    std::unique_ptr<Encoder> encoder_;
    std::unique_ptr<Decoder> decoder_;
};

struct ProcessingStats {
    size_t frames_processed = 0;
    size_t bytes_encoded = 0;
    size_t bytes_decoded = 0;
    double total_encode_time_ms = 0.0;
    double total_decode_time_ms = 0.0;
    size_t crc_errors = 0;
    size_t erased_frames = 0;
};

class CodecProcessor {
public:
    using ProgressCallback = std::function<void(float progress, const ProcessingStats& stats)>;
    
    CodecProcessor(const EncoderConfig& enc_config = {}, const DecoderConfig& dec_config = {});
    ~CodecProcessor();
    
    [[nodiscard]] Result<ProcessingStats> process_file(
        const std::string& input_path,
        const std::string& output_path,
        bool encode = true,
        ProgressCallback callback = nullptr
    );
    
    [[nodiscard]] Result<ProcessingStats> process_stream(
        std::istream& input,
        std::ostream& output,
        bool encode = true,
        ProgressCallback callback = nullptr
    );

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace g723