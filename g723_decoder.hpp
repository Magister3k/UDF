#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include <array>
#include <span>
#include <optional>
#include <string>

namespace g723_decoder {

using Word16 = int16_t;
using Word32 = int32_t;
using Flag = int;
using Float = double;

constexpr bool False = false;
constexpr bool True = true;

enum class FrameType : uint8_t {
    Untransmitted = 0,
    Active = 1,
    SID = 2
};

enum class CodecRate : uint8_t {
    Rate63 = 0,
    Rate53 = 1
};

struct DecoderConfig {
    bool use_pf = true;
};

struct AudioFrame {
    std::array<Float, 240> samples{};
    size_t valid_samples = 240;
};

struct BitstreamFrame {
    std::vector<uint8_t> data;
    FrameType type = FrameType::Untransmitted;
    CodecRate rate = CodecRate::Rate63;
    bool crc_error = false;
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

class Decoder {
public:
    explicit Decoder(const DecoderConfig& config = {});
    ~Decoder();
    
    Decoder(const Decoder&) = delete;
    Decoder& operator=(const Decoder&) = delete;
    Decoder(Decoder&&) noexcept;
    Decoder& operator=(Decoder&&) noexcept;
    
    Result<AudioFrame> decode(const BitstreamFrame& frame);
    Result<AudioFrame> decode(std::span<const uint8_t> data, FrameType type, CodecRate rate, bool crc = false);
    
    void reset();
    bool get_use_pf() const;
    void set_use_pf(bool use_pf);

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace g723_decoder