#ifndef JPG_BLOCK_ENCODER_HPP 
#define JPG_BLOCK_ENCODER_HPP

#include <vector>
#include <cstdint>
#include <cmath>
#include <numeric>
#include <map> 

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

// Tabla de escaneo Zig-Zag
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
    for (int i = 0; i < 8; ++i) {
        for (int j = 0; j < 8; ++j) {
            double ci = (i == 0) ? 1.0 / std::sqrt(2.0) : 1.0;
            double cj = (j == 0) ? 1.0 / std::sqrt(2.0) : 1.0;
            dctFactors[i * 8 + j] = ci * cj / 4.0; 
        }
    }
    dctFactorsInitialized = true; // Marcar como inicializado
}

void perform_dct(const std::vector<double>& inputBlock, std::vector<double>& outputCoeffs) {
    if (!dctFactorsInitialized) {
        initDctFactors();
    }
    outputCoeffs.assign(64, 0.0);
    for (int u = 0; u < 8; ++u) {
        for (int v = 0; v < 8; ++v) {
            double sum = 0.0;
            for (int x = 0; x < 8; ++x) {
                for (int y = 0; y < 8; ++y) {
                    sum += inputBlock[x * 8 + y] * 
                           std::cos((2.0 * x + 1.0) * u * PI / 16.0) * 
                           std::cos((2.0 * y + 1.0) * v * PI / 16.0);
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
        quantizedCoeffs[i] = static_cast<int>(std::round(dctCoeffs[i] / quantTable[i]));
    }
}

// --- Codificación Huffman ---

inline void emit_bit(uint8_t bit, std::vector<uint8_t>& out_bytes, uint8_t& current_byte, int& bits_in_byte) {
    current_byte = (current_byte << 1) | bit;
    bits_in_byte++;
    if (bits_in_byte == 8) {
        out_bytes.push_back(current_byte);
        if (current_byte == 0xFF) { 
            out_bytes.push_back(0x00);
        }
        current_byte = 0;
        bits_in_byte = 0;
    }
}

inline std::vector<uint8_t> jpeg_encode_magnitude(int value, uint8_t& category) {
    std::vector<uint8_t> bits;
    if (value == 0) { category = 0; return bits; }
    category = static_cast<uint8_t>(std::floor(std::log2(std::abs(value))) + 1);
    int magnitude_val = value;
    if (value < 0) { magnitude_val = (1 << category) - 1 + value; }
    for (int i = category - 1; i >= 0; --i) { bits.push_back((magnitude_val >> i) & 1); }
    return bits;
}

// Tablas Huffman DC
const uint16_t huff_codes_dc_lum[] = {0x00,0x02,0x03,0x04,0x05,0x06,0x0E,0x1E,0x3E,0x7E,0xFE,0x1FE};
const uint8_t huff_sizes_dc_lum[] = {2,3,3,3,3,3,4,5,6,7,8,9};
const uint16_t k4_huff_codes_dc_chrom[] = {0x00,0x01,0x02,0x06,0x0E,0x1E,0x3E,0x7E,0xFE,0x1FE,0x3FE,0x7FE};
const uint8_t k4_huff_sizes_dc_chrom[] = {2,2,2,3,4,5,6,7,8,9,10,11};

// Mapas Globales para Huffman AC (se inicializarán desde main.cpp)
std::map<uint8_t, std::pair<uint16_t, uint8_t>> huff_ac_lum_map;
std::map<uint8_t, std::pair<uint16_t, uint8_t>> huff_ac_chrom_map;
bool huffAcMapsInitialized = false; // Flag para asegurar inicialización única

// Función para construir mapas Huffman AC (llamada desde main.cpp)
void buildHuffmanACTables(
    const uint8_t bits[16], const uint8_t* values, size_t num_values, // Usar size_t
    std::map<uint8_t, std::pair<uint16_t, uint8_t>>& huff_map) 
{
    huff_map.clear();
    uint16_t code = 0;
    int k = 0;
    for (int i = 1; i <= 16; ++i) { 
        for (int j = 0; j < bits[i - 1]; ++j) { 
            if (k < num_values) {
                huff_map[values[k]] = {code, static_cast<uint8_t>(i)};
                k++;
            }
            code++;
        }
        code <<= 1; 
    }
}


// Función principal para codificar un bloque completo (Y, Cb, o Cr)
// AHORA CON PARÁMETROS SIMPLIFICADOS
void encode_jpeg_block(
    const std::vector<double>& inputBlock, // Bloque 8x8 de entrada (centrado en 0)
    const uint8_t* quantTable,           // Tabla de cuantización para este componente
    int& prev_dc_quantized,              // DC cuantizado previo (entrada/salida)
    bool is_luma,                        // ¿Es luminancia?
    // Estado del flujo de bits (compartido)
    std::vector<uint8_t>& out_bytes, 
    uint8_t& shared_current_byte,    
    int& shared_bits_in_byte) 
{
    // Asegurarse de que los mapas AC estén inicializados (la inicialización real ocurre en main)
    if (!huffAcMapsInitialized) {
         // Error: Las tablas AC deberían haberse inicializado en main.cpp
         // Podrías lanzar una excepción o imprimir un error aquí.
         std::cerr << "Error: Mapas Huffman AC no inicializados antes de llamar a encode_jpeg_block!" << std::endl;
         return; 
    }

    // 1. DCT
    std::vector<double> dctCoeffs;
    perform_dct(inputBlock, dctCoeffs);

    // 2. Cuantización
    std::vector<int> quantizedCoeffs;
    quantize(dctCoeffs, quantTable, quantizedCoeffs);

    // 3. Codificación DC
    int dc_diff = quantizedCoeffs[0] - prev_dc_quantized;
    prev_dc_quantized = quantizedCoeffs[0];
    
    uint8_t category_dc;
    std::vector<uint8_t> dc_magnitude_bits = jpeg_encode_magnitude(dc_diff, category_dc);
    
    const uint16_t* huff_codes_dc_table = is_luma ? huff_codes_dc_lum : k4_huff_codes_dc_chrom;
    const uint8_t*  huff_sizes_dc_table = is_luma ? huff_sizes_dc_lum : k4_huff_sizes_dc_chrom;

    // Validar categoría DC antes de acceder a la tabla
    uint8_t max_category_dc = (sizeof(huff_sizes_dc_lum) / sizeof(huff_sizes_dc_lum[0])) -1; // Asumiendo que Luma tiene más o igual categorías
    if (category_dc > max_category_dc) {
         std::cerr << "Error: Categoría DC fuera de rango: " << (int)category_dc << std::endl;
         category_dc = max_category_dc; // Intentar recuperarse usando la categoría máxima? O manejar error
    }

    uint16_t dc_huff_code = huff_codes_dc_table[category_dc];
    uint8_t  dc_huff_size = huff_sizes_dc_table[category_dc];

    for (int i = dc_huff_size - 1; i >= 0; --i) {
        emit_bit((dc_huff_code >> i) & 1, out_bytes, shared_current_byte, shared_bits_in_byte);
    }
    for (uint8_t bit : dc_magnitude_bits) {
        emit_bit(bit, out_bytes, shared_current_byte, shared_bits_in_byte);
    }

    // 4. Codificación AC (ZigZag + RLE + Huffman)
    // Seleccionar el mapa AC correcto (Luma o Chroma)
    const auto& huff_map_ac = is_luma ? huff_ac_lum_map : huff_ac_chrom_map;
    int zero_run_length = 0;

    for (int i = 1; i < 64; ++i) { 
        int ac_val = quantizedCoeffs[zigZagMap[i]];

        if (ac_val == 0) {
            zero_run_length++;
        } else {
            while (zero_run_length >= 16) { 
                auto it_zrl = huff_map_ac.find(0xF0); // Símbolo ZRL = (15, 0)
                 if (it_zrl != huff_map_ac.end()) {
                    uint16_t ac_huff_code = it_zrl->second.first;
                    uint8_t ac_huff_size = it_zrl->second.second;
                    for (int k = ac_huff_size - 1; k >= 0; --k) {
                        emit_bit((ac_huff_code >> k) & 1, out_bytes, shared_current_byte, shared_bits_in_byte);
                    }
                 } else { 
                     std::cerr << "Error: ZRL (0xF0) no encontrado en tabla Huffman AC " << (is_luma ? "Luma" : "Chroma") << std::endl; 
                 }
                zero_run_length -= 16;
            }

            uint8_t category_ac;
            std::vector<uint8_t> ac_magnitude_bits = jpeg_encode_magnitude(ac_val, category_ac);
            uint8_t run_size_symbol = (static_cast<uint8_t>(zero_run_length) << 4) | category_ac;

            auto it = huff_map_ac.find(run_size_symbol);
            if (it != huff_map_ac.end()) {
                 uint16_t ac_huff_code = it->second.first;
                 uint8_t ac_huff_size = it->second.second;

                 for (int k = ac_huff_size - 1; k >= 0; --k) {
                     emit_bit((ac_huff_code >> k) & 1, out_bytes, shared_current_byte, shared_bits_in_byte);
                 }
                 for (uint8_t bit : ac_magnitude_bits) {
                     emit_bit(bit, out_bytes, shared_current_byte, shared_bits_in_byte);
                 }
            } else { 
                 std::cerr << "Error: Símbolo AC (run,size) 0x" << std::hex << (int)run_size_symbol 
                           << " no encontrado en tabla Huffman AC " << (is_luma ? "Luma" : "Chroma") << std::endl; 
            }
            zero_run_length = 0; 
        }
    }

    if (zero_run_length > 0) {
        auto it_eob = huff_map_ac.find(0x00); // Símbolo EOB = (0, 0)
        if (it_eob != huff_map_ac.end()) {
             uint16_t eob_huff_code = it_eob->second.first;
             uint8_t eob_huff_size = it_eob->second.second;
             for (int k = eob_huff_size - 1; k >= 0; --k) {
                 emit_bit((eob_huff_code >> k) & 1, out_bytes, shared_current_byte, shared_bits_in_byte);
             }
        } else { 
             std::cerr << "Error: EOB (0x00) no encontrado en tabla Huffman AC " << (is_luma ? "Luma" : "Chroma") << std::endl; 
        }
    }
}

} // Fin del namespace JpegEncoder

#endif