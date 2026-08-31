#include "g723_codec.hpp"
#include "g723_types.hpp"
#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>

using namespace g723;

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <input.pcm> <output.g723>\n";
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

    EncoderConfig config;
    config.rate = CodecRate::Rate63;
    config.use_hp = true;
    config.use_vad = true;
    config.use_pf = true;
    Encoder encoder(config);

    std::vector<int16_t> pcm_buffer(240);
    size_t frame_count = 0;
    size_t total_bytes = 0;

    while (input.read(reinterpret_cast<char*>(pcm_buffer.data()), 240 * sizeof(int16_t))) {
        std::vector<Float> float_buffer(240);
        for (int i = 0; i < 240; ++i) {
            float_buffer[i] = static_cast<Float>(pcm_buffer[i]);
        }

        auto result = encoder.encode(float_buffer);
        if (!result) {
            std::cerr << "Encode error on frame " << frame_count << ": " << result.error() << std::endl;
            return 1;
        }

        output.write(reinterpret_cast<const char*>(result->data.data()), result->data.size());
        total_bytes += result->data.size();
        frame_count++;
    }

    std::cerr << "Encoded " << frame_count << " frames (" 
              << (frame_count * 30) << " ms), " << total_bytes << " bytes" << std::endl;
    
    return 0;
}