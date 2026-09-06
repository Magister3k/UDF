#include "g723_codec.hpp"
#include "g723_types.hpp"
#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>

using namespace g723;

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <input.g723> <output.pcm>\n";
        return 1;
    }

    const char* input_path = argv[1];
    const char* output_path = argv[2];

    std::ifstream input(input_path, std::ios::binary);
    if (!input) {
        std::cerr << "Error: Cannot open input file: " << input_path << std::endl;
        return 1;
    }

    std::ofstream output(output_path, std::ios::binary);
    if (!output) {
        std::cerr << "Error: Cannot open output file: " << output_path << std::endl;
        return 1;
    }

    DecoderConfig config;
    config.use_pf = true;
    Decoder decoder(config);

    uint8_t header;
    size_t frame_count = 0;
    size_t total_bytes = 0;

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
            if (input.gcount() != frame_size - 1) {
                std::cerr << "Warning: Incomplete frame at end of file" << std::endl;
                break;
            }
        }

        BitstreamFrame frame;
        frame.data = std::move(frame_data);
        frame.type = type;
        frame.rate = rate;

        auto result = decoder.decode(frame);
        if (!result) {
            std::cerr << "Decode error on frame " << frame_count << ": " << result.error() << std::endl;
            return 1;
        }

        std::vector<int16_t> pcm_out(240);
        for (int i = 0; i < 240; ++i) {
            Float val = result->samples[i];
            if (val < -32767.5) val = -32768.0;
            else if (val > 32766.5) val = 32767.0;
            pcm_out[i] = (val >= 0) ? static_cast<int16_t>(val + 0.5) : static_cast<int16_t>(val - 0.5);
        }

        output.write(reinterpret_cast<const char*>(pcm_out.data()), 240 * sizeof(int16_t));
        total_bytes += frame.data.size();
        frame_count++;
    }

    std::cerr << "Decoded " << frame_count << " frames (" 
              << (frame_count * 30) << " ms), " << total_bytes << " bytes" << std::endl;
    
    return 0;
}