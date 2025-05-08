#ifndef JPG_BLOCK_ENCODER_HPP 
#define JPG_BLOCK_ENCODER_HPP

#include <vector>
#include <cstdint>
#include <cmath>
#include <numeric>
#include <map> 
#include <iostream> // Para errores
#include <iomanip>  // Para std::hex

// Ayudante para asegurar que std::round esté disponible
#if __cplusplus < 201103L
namespace std {
    inline double round(double val) {
        return floor(val + 0.5);
    }
}
#endif

namespace JpegEncoder {

// --- Constantes y Tablas ---
const double PI = 3.14159265358979323846;

const int zigZagMap[64] = {
     0,  1,  5,  6, 14, 15, 27, 28, 2,  4,  7, 13, 16, 26, 29, 42,
     3,  8, 12, 17, 25, 30, 41, 43, 9, 11, 18, 24, 31, 40, 44, 53,
    10, 19, 23, 32, 39, 45, 52, 54, 20, 22, 33, 38, 46, 51, 55, 60,
    21, 34, 37, 47, 50, 56, 59, 61, 35, 36, 48, 49, 57, 58, 62, 63
};

// --- Implementación DCT ---
std::vector<double> dctFactors(64);
bool dctFactorsInitialized = false;
void initDctFactors() {
    if (dctFactorsInitialized) return; // Evitar reinicialización
    for (int i = 0; i < 8; ++i) {
        for (int j = 0; j < 8; ++j) {
            double ci = (i == 0) ? 1.0 / std::sqrt(2.0) : 1.0;
            double cj = (j == 0) ? 1.0 / std::sqrt(2.0) : 1.0;
            dctFactors[i * 8 + j] = ci * cj / 4.0; 
        }
    }
    dctFactorsInitialized = true; 
}

void perform_dct(const std::vector<double>& inputBlock, std::vector<double>& outputCoeffs) {
    if (!dctFactorsInitialized) { initDctFactors(); }
    outputCoeffs.assign(64, 0.0);
    for (int u = 0; u < 8; ++u) {
        for (int v = 0; v < 8; ++v) {
            double sum = 0.0;
            for (int x = 0; x < 8; ++x) {
                for (int y = 0; y < 8; ++y) {
                    sum += inputBlock[x * 8 + y] * std::cos((2.0*x+1.0)*u*PI/16.0) * std::cos((2.0*y+1.0)*v*PI/16.0);
                }
            }
            outputCoeffs[u * 8 + v] = dctFactors[u * 8 + v] * sum;
        }
    }
}

// --- Cuantización ---
void quantize(const std::vector<double>& dctCoeffs, const uint8_t* quantTable, std::vector<int>& quantizedCoeffs) {
    quantizedCoeffs.assign(64, 0);
    for (int i = 0; i < 64; ++i) {
        // Asegurarse de que el valor de la tabla de cuantización no sea cero para evitar división por cero
        int q_val = (quantTable[i] == 0) ? 1 : quantTable[i];
        quantizedCoeffs[i] = static_cast<int>(std::round(dctCoeffs[i] / q_val));
    }
}

// --- Codificación Huffman ---
inline void emit_bit(uint8_t bit, std::vector<uint8_t>& out_bytes, uint8_t& current_byte, int& bits_in_byte) {
    current_byte = (current_byte << 1) | bit;
    bits_in_byte++;
    if (bits_in_byte == 8) {
        out_bytes.push_back(current_byte);
        if (current_byte == 0xFF) { out_bytes.push_back(0x00); }
        current_byte = 0; bits_in_byte = 0;
    }
}

inline std::vector<uint8_t> jpeg_encode_magnitude(int value, uint8_t& category) {
    std::vector<uint8_t> bits;
    if (value == 0) { category = 0; return bits; }
    category = static_cast<uint8_t>(std::floor(std::log2(std::abs(value))) + 1);
    if (category > 11) category = 11; // Limitar categoría si es necesario (las tablas estándar suelen llegar hasta 11)
    int magnitude_val = value;
    if (value < 0) { magnitude_val = (1 << category) - 1 + value; }
    for (int i = category - 1; i >= 0; --i) { bits.push_back((magnitude_val >> i) & 1); }
    return bits;
}

const uint16_t huff_codes_dc_lum[] = {0x00,0x02,0x03,0x04,0x05,0x06,0x0E,0x1E,0x3E,0x7E,0xFE,0x1FE};
const uint8_t huff_sizes_dc_lum[] = {2,3,3,3,3,3,4,5,6,7,8,9};
const uint16_t k4_huff_codes_dc_chrom[] = {0x00,0x01,0x02,0x06,0x0E,0x1E,0x3E,0x7E,0xFE,0x1FE,0x3FE,0x7FE};
const uint8_t k4_huff_sizes_dc_chrom[] = {2,2,2,3,4,5,6,7,8,9,10,11};

std::map<uint8_t, std::pair<uint16_t, uint8_t>> huff_ac_lum_map;
std::map<uint8_t, std::pair<uint16_t, uint8_t>> huff_ac_chrom_map;
bool huffAcMapsInitialized = false; 

void buildHuffmanACTables(const uint8_t bits[16], const uint8_t* values, size_t num_values, 
                          std::map<uint8_t, std::pair<uint16_t, uint8_t>>& huff_map) {
    huff_map.clear(); uint16_t code = 0; int k = 0;
    for (int i = 1; i <= 16; ++i) { 
        for (int j = 0; j < bits[i - 1]; ++j) { 
            if (k < num_values) { huff_map[values[k]] = {code, (uint8_t)i}; k++; }
            code++;
        }
        code <<= 1; 
    }
}

void encode_jpeg_block(const std::vector<double>& inputBlock, const uint8_t* quantTable, int& prev_dc_quantized, bool is_luma,
                       std::vector<uint8_t>& out_bytes, uint8_t& shared_current_byte, int& shared_bits_in_byte) {
    if (!huffAcMapsInitialized) { std::cerr << "Error: Mapas Huffman AC no inicializados!" << std::endl; return; }

    std::vector<double> dctCoeffs; perform_dct(inputBlock, dctCoeffs);
    std::vector<int> quantizedCoeffs; quantize(dctCoeffs, quantTable, quantizedCoeffs);

    int dc_diff = quantizedCoeffs[0] - prev_dc_quantized; prev_dc_quantized = quantizedCoeffs[0];
    uint8_t category_dc; std::vector<uint8_t> dc_magnitude_bits = jpeg_encode_magnitude(dc_diff, category_dc);
    const uint16_t* huff_codes_dc = is_luma ? huff_codes_dc_lum : k4_huff_codes_dc_chrom;
    const uint8_t* huff_sizes_dc = is_luma ? huff_sizes_dc_lum : k4_huff_sizes_dc_chrom;
    uint8_t max_category_dc = (sizeof(huff_sizes_dc_lum)/sizeof(huff_sizes_dc_lum[0])) - 1;
    if (category_dc > max_category_dc) { category_dc = max_category_dc; } // Simple clamp
    uint16_t dc_code = huff_codes_dc[category_dc]; uint8_t dc_size = huff_sizes_dc[category_dc];
    for (int i = dc_size - 1; i >= 0; --i) emit_bit((dc_code >> i) & 1, out_bytes, shared_current_byte, shared_bits_in_byte);
    for (uint8_t bit : dc_magnitude_bits) emit_bit(bit, out_bytes, shared_current_byte, shared_bits_in_byte);

    const auto& huff_map_ac = is_luma ? huff_ac_lum_map : huff_ac_chrom_map;
    int zero_run = 0;
    for (int i = 1; i < 64; ++i) { 
        int ac_val = quantizedCoeffs[zigZagMap[i]];
        if (ac_val == 0) { zero_run++; } 
        else {
            while (zero_run >= 16) { 
                auto it = huff_map_ac.find(0xF0); // ZRL
                if(it != huff_map_ac.end()){ uint16_t c=it->second.first; uint8_t s=it->second.second; for(int k=s-1;k>=0;--k) emit_bit((c>>k)&1, out_bytes, shared_current_byte, shared_bits_in_byte); } else { std::cerr<<"E:ZRL "; }
                zero_run -= 16;
            }
            uint8_t cat_ac; std::vector<uint8_t> ac_mag_bits = jpeg_encode_magnitude(ac_val, cat_ac);
            uint8_t sym = (uint8_t(zero_run) << 4) | cat_ac;
            auto it = huff_map_ac.find(sym);
            if(it != huff_map_ac.end()){ uint16_t c=it->second.first; uint8_t s=it->second.second; for(int k=s-1;k>=0;--k) emit_bit((c>>k)&1, out_bytes, shared_current_byte, shared_bits_in_byte); for(uint8_t b:ac_mag_bits) emit_bit(b, out_bytes, shared_current_byte, shared_bits_in_byte); } else { std::cerr<<"E:AC"<<std::hex<<(int)sym<<" "; }
            zero_run = 0; 
        }
    }
    if (zero_run > 0) { 
        auto it = huff_map_ac.find(0x00); // EOB
        if(it != huff_map_ac.end()){ uint16_t c=it->second.first; uint8_t s=it->second.second; for(int k=s-1;k>=0;--k) emit_bit((c>>k)&1, out_bytes, shared_current_byte, shared_bits_in_byte); } else { std::cerr<<"E:EOB "; }
    }
}

} // namespace JpegEncoder
#endif