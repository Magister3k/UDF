#pragma once

#include "g723_types.hpp"
#include <memory>
#include <functional>

namespace g723 {

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

class CodecProcessor {
public:
    using ProgressCallback = std::function<void(float progress, const ProcessingStats& stats)>;

    CodecProcessor(const DecoderConfig& dec_config = {});
    ~CodecProcessor();

    [[nodiscard]] Result<ProcessingStats> process_file(
        const std::string& input_path,
        const std::string& output_path,
        ProgressCallback callback = nullptr
    );

    [[nodiscard]] Result<ProcessingStats> process_stream(
        std::istream& input,
        std::ostream& output,
        ProgressCallback callback = nullptr
    );

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace g723