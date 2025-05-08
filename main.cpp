//Este es el script principal para mi motor gráfico
#include <iostream>
#include <string>
#include <cstdint>
#include <algorithm>
#include <fstream>
#include <vector>
#include <cmath>
#include <iomanip> 
#include <stdexcept> // Para std::invalid_argument, std::out_of_range
#include "jpg_encode.hpp" 

// --- Funciones de Conversión de Color ---
struct RGB { uint8_t r, g, b; }; struct YCbCr { int y, cb, cr; };
RGB hsv_to_rgb(double h, double s, double v) { /* ... como antes ... */ 
    double r_f=0,g_f=0,b_f=0; if(s==0){r_f=g_f=b_f=v;}else{double i=std::floor(h/60.0);double f=(h/60.0)-i;double p=v*(1.0-s);double q=v*(1.0-s*f);double t=v*(1.0-s*(1.0-f));switch(static_cast<int>(i)%6){case 0:r_f=v;g_f=t;b_f=p;break;case 1:r_f=q;g_f=v;b_f=p;break;case 2:r_f=p;g_f=v;b_f=t;break;case 3:r_f=p;g_f=q;b_f=v;break;case 4:r_f=t;g_f=p;b_f=v;break;case 5:r_f=v;g_f=p;b_f=q;break;}}
    return {(uint8_t)std::round(r_f*255.0),(uint8_t)std::round(g_f*255.0),(uint8_t)std::round(b_f*255.0)};
}
YCbCr rgb_to_ycbcr(RGB color) { /* ... como antes ... */ 
    double y_f=0.299*color.r+0.587*color.g+0.114*color.b; double cb_f=-0.168736*color.r-0.331264*color.g+0.5*color.b+128.0; double cr_f=0.5*color.r-0.418688*color.g-0.081312*color.b+128.0;
    return {(int)std::round(std::max(0.0,std::min(255.0,y_f))),(int)std::round(std::max(0.0,std::min(255.0,cb_f))),(int)std::round(std::max(0.0,std::min(255.0,cr_f)))};
}

//Función para escribir al binario de un bitmap (Espectro de Colores)
int write_bmp() { 
    uint8_t h[54]; int X=500; int Y=500; const int d=3; const int sz=(((X*d)+3)/4)*4*Y; const int fsz=54+sz;
    h[0]='B';h[1]='M';h[2]=fsz&0xFF;h[3]=(fsz>>8)&0xFF;h[4]=(fsz>>16)&0xFF;h[5]=(fsz>>24)&0xFF;std::fill(h+6,h+10,0);h[10]=54;std::fill(h+11,h+14,0);h[14]=40;std::fill(h+15,h+18,0);
    h[18]=X&0xFF;h[19]=(X>>8)&0xFF;h[20]=(X>>16)&0xFF;h[21]=(X>>24)&0xFF;h[22]=Y&0xFF;h[23]=(Y>>8)&0xFF;h[24]=(Y>>16)&0xFF;h[25]=(Y>>24)&0xFF;
    h[26]=1;h[27]=0;h[28]=24;h[29]=0;std::fill(h+30,h+34,0);h[34]=sz&0xFF;h[35]=(sz>>8)&0xFF;h[36]=(sz>>16)&0xFF;h[37]=(sz>>24)&0xFF;std::fill(h+38,h+54,0);
    std::ofstream a("imagen.bmp",std::ios::binary); if(!a){std::cerr<<"Error al abrir imagen.bmp"<<std::endl; return 1;}
    a.write(reinterpret_cast<char*>(h),54);int p=(((X*d)+3)/4)*4-(X*d);
    for(int y=0;y<Y;++y){for(int x=0;x<X;++x){double hu=(double(x)/(X-1.0))*360.0;double s=1.0;double v=1.0-(double(y)/(Y-1.0));RGB px=hsv_to_rgb(hu,s,v);a.put(px.b);a.put(px.g);a.put(px.r);}for(int i=0;i<p;++i)a.put(0);}
    a.close(); std::cout<<"BMP (espectro): Creado."<<std::endl; return 0;
}

// --- Funciones para Escalado de Calidad JPEG ---
void scale_quantization_table(const uint8_t base_table[64], int quality, uint8_t scaled_table[64]) {
    int scale_factor;
    if (quality <= 0) quality = 1;
    if (quality > 100) quality = 100;

    if (quality < 50) {
        scale_factor = 5000 / quality;
    } else {
        scale_factor = 200 - 2 * quality;
    }

    for (int i = 0; i < 64; ++i) {
        long temp = ((long)base_table[i] * scale_factor + 50) / 100;
        // Limitar valores al rango [1, 255]
        if (temp <= 0) temp = 1;
        if (temp > 255) temp = 255; 
        scaled_table[i] = (uint8_t)temp;
    }
}


//Vale, ya podemos hacer imágenes pero de un cuadrado rojo, vamos al menos a hacer un conversor de imágenes, asi que para hacer un converor de imágenes
//de jpg a bmp y viceversa
//En primer caso voy a definir la función para comprimir imágenes de jpg, es bastante complejo no merece la pena aprenderlo simplemente copy paste
int write_jpg(int quality_percent = -1) { // -1 indica usar calidad predeterminada alta
    const int resX = 500;
    const int resY = 500;

    std::vector<uint8_t> jpeg_data;
    auto append_bytes = [&](const uint8_t* data, size_t size) { jpeg_data.insert(jpeg_data.end(), data, data + size); };
    auto append_byte = [&](uint8_t byte) { jpeg_data.push_back(byte); };

    append_byte(0xFF); append_byte(0xD8); // SOI

    const uint8_t app0_data[] = { 0xFF,0xE0,0x00,0x10,0x4A,0x46,0x49,0x46,0x00,0x01,0x01,0x00,0x00,0x01,0x00,0x01,0x00,0x00 };
    append_bytes(app0_data, sizeof(app0_data));

    // Tablas DQT base (Estándar con DC=16/17)
    const uint8_t dqt_luma_base[] = { 16,11,10,16,24,40,51,61,12,12,14,19,26,58,60,55,14,13,16,24,40,57,69,56,14,17,22,29,51,87,80,62,18,22,37,56,68,109,103,77,24,35,55,64,81,104,113,92,49,64,78,87,103,121,120,101,72,92,95,98,112,100,103,99 };
    const uint8_t dqt_chroma_base[] = { 17,18,24,47,99,99,99,99,18,21,26,66,99,99,99,99,24,26,56,99,99,99,99,99,47,66,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99 };
    
    // Tablas DQT de alta calidad predeterminada (DC=1)
    const uint8_t dqt_luma_high_q[] = { 1,11,10,16,24,40,51,61,12,12,14,19,26,58,60,55,14,13,16,24,40,57,69,56,14,17,22,29,51,87,80,62,18,22,37,56,68,109,103,77,24,35,55,64,81,104,113,92,49,64,78,87,103,121,120,101,72,92,95,98,112,100,103,99 };
    const uint8_t dqt_chroma_high_q[] = { 1,18,24,47,99,99,99,99,18,21,26,66,99,99,99,99,24,26,56,99,99,99,99,99,47,66,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99 };

    // Tablas DQT finales que se usarán
    uint8_t dqt_luma_final[64];
    uint8_t dqt_chroma_final[64];

    if (quality_percent == -1) { // Usar predeterminada alta
        std::copy(dqt_luma_high_q, dqt_luma_high_q + 64, dqt_luma_final);
        std::copy(dqt_chroma_high_q, dqt_chroma_high_q + 64, dqt_chroma_final);
        std::cout << "Usando calidad JPEG predeterminada (alta)." << std::endl;
    } else { // Escalar tablas base según el porcentaje
        scale_quantization_table(dqt_luma_base, quality_percent, dqt_luma_final);
        scale_quantization_table(dqt_chroma_base, quality_percent, dqt_chroma_final);
        std::cout << "Usando calidad JPEG: " << quality_percent << "%" << std::endl;
    }

    // Escribir segmento DQT con las tablas finales
    uint16_t dqt_len = 2+(1+64)+(1+64); append_byte(0xFF); append_byte(0xDB); append_byte((dqt_len>>8)&0xFF); append_byte(dqt_len&0xFF);
    append_byte(0x00); append_bytes(dqt_luma_final, 64); // Luma final
    append_byte(0x01); append_bytes(dqt_chroma_final, 64); // Chroma final

    // SOF0 (Sin cambios)
    uint16_t sof0_len = 8+3*3; append_byte(0xFF); append_byte(0xC0); append_byte((sof0_len>>8)&0xFF); append_byte(sof0_len&0xFF); append_byte(0x08); append_byte((resY>>8)&0xFF); append_byte(resY&0xFF); append_byte((resX>>8)&0xFF); append_byte(resX&0xFF); append_byte(0x03); append_byte(0x01); append_byte(0x11); append_byte(0x00); append_byte(0x02); append_byte(0x11); append_byte(0x01); append_byte(0x03); append_byte(0x11); append_byte(0x01); 
    
    // DHT (Usar las tablas estándar definidas globalmente en el namespace)
    const uint8_t bits_dc_lum[]={0x00,0x01,0x05,0x01,0x01,0x01,0x01,0x01,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00}; const uint8_t val_dc_lum[]={0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B};
    const uint8_t bits_ac_lum[]={0x00,0x02,0x01,0x03,0x03,0x02,0x04,0x03,0x05,0x05,0x04,0x04,0x00,0x00,0x01,0x7D}; const uint8_t val_ac_lum[]={0x01,0x02,0x03,0x00,0x04,0x11,0x05,0x12,0x21,0x31,0x41,0x06,0x13,0x51,0x61,0x07,0x22,0x71,0x14,0x32,0x81,0x91,0xA1,0x08,0x23,0x42,0xB1,0xC1,0x15,0x52,0xD1,0xF0,0x24,0x33,0x62,0x72,0x82,0x09,0x0A,0x16,0x17,0x18,0x19,0x1A,0x25,0x26,0x27,0x28,0x29,0x2A,0x34,0x35,0x36,0x37,0x38,0x39,0x3A,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4A,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5A,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6A,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7A,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8A,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9A,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xE1,0xE2,0xE3,0xE4,0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0xFA};
    const uint8_t bits_dc_chrom[]={0x00,0x03,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x00,0x00,0x00,0x00,0x00}; const uint8_t val_dc_chrom[]={0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B};
    const uint8_t bits_ac_chrom[]={0x00,0x02,0x01,0x02,0x04,0x04,0x03,0x04,0x07,0x05,0x04,0x04,0x00,0x01,0x02,0x77}; const uint8_t val_ac_chrom[]={0x00,0x01,0x02,0x03,0x11,0x04,0x05,0x21,0x31,0x06,0x12,0x41,0x51,0x07,0x61,0x71,0x13,0x22,0x32,0x81,0x08,0x14,0x42,0x91,0xA1,0xB1,0xC1,0x09,0x23,0x33,0x52,0xF0,0x15,0x62,0x72,0xD1,0x0A,0x16,0x24,0x34,0xE1,0x25,0xF1,0x17,0x18,0x19,0x1A,0x26,0x27,0x28,0x29,0x2A,0x35,0x36,0x37,0x38,0x39,0x3A,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4A,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5A,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6A,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7A,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8A,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9A,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xE2,0xE3,0xE4,0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0xFA};
    
    // Inicializar mapas Huffman AC (solo si no están ya inicializados)
    if (!JpegEncoder::huffAcMapsInitialized) { 
         JpegEncoder::buildHuffmanACTables(bits_ac_lum, val_ac_lum, sizeof(val_ac_lum), JpegEncoder::huff_ac_lum_map);
         JpegEncoder::buildHuffmanACTables(bits_ac_chrom, val_ac_chrom, sizeof(val_ac_chrom), JpegEncoder::huff_ac_chrom_map);
         JpegEncoder::huffAcMapsInitialized = true;
    }
    // Escribir segmento DHT
    uint16_t dht_len=2+(1+16+sizeof(val_dc_lum))+(1+16+sizeof(val_ac_lum))+(1+16+sizeof(val_dc_chrom))+(1+16+sizeof(val_ac_chrom)); append_byte(0xFF); append_byte(0xC4); append_byte((dht_len>>8)&0xFF); append_byte(dht_len&0xFF);
    append_byte(0x00); append_bytes(bits_dc_lum, 16); append_bytes(val_dc_lum, sizeof(val_dc_lum));
    append_byte(0x10); append_bytes(bits_ac_lum, 16); append_bytes(val_ac_lum, sizeof(val_ac_lum));
    append_byte(0x01); append_bytes(bits_dc_chrom, 16); append_bytes(val_dc_chrom, sizeof(val_dc_chrom));
    append_byte(0x11); append_bytes(bits_ac_chrom, 16); append_bytes(val_ac_chrom, sizeof(val_ac_chrom));
    
    // SOS (Sin cambios)
    uint16_t sos_len = 6+2*3; append_byte(0xFF); append_byte(0xDA); append_byte((sos_len>>8)&0xFF); append_byte(sos_len&0xFF); append_byte(0x03); append_byte(0x01); append_byte(0x00); append_byte(0x02); append_byte(0x11); append_byte(0x03); append_byte(0x11); append_byte(0x00); append_byte(0x3F); append_byte(0x00); 

    uint8_t mcu_current_byte = 0; int mcu_bits_in_current_byte = 0; 
    int num_blocks_x = (resX + 7) / 8; int num_blocks_y = (resY + 7) / 8; int total_blocks = num_blocks_x * num_blocks_y; 
    int prev_dc_Y=0, prev_dc_Cb=0, prev_dc_Cr=0; 
    std::vector<double> block_Y(64), block_Cb(64), block_Cr(64);

    for (int y_mcu = 0; y_mcu < num_blocks_y; ++y_mcu) {
        for (int x_mcu = 0; x_mcu < num_blocks_x; ++x_mcu) {
            for (int y=0; y<8; ++y) { for (int x=0; x<8; ++x) {
                int gx=x_mcu*8+x, gy=y_mcu*8+y; if(gx>=resX||gy>=resY){gx=x_mcu*8;gy=y_mcu*8;} // Borde simple
                double h=(double(gx)/(resX-1.))*360., s=1., v=1.-(double(gy)/(resY-1.));
                RGB rgb=hsv_to_rgb(h,s,v); YCbCr ycbcr=rgb_to_ycbcr(rgb); int idx=y*8+x;
                block_Y[idx]=double(ycbcr.y)-128.; block_Cb[idx]=double(ycbcr.cb)-128.; block_Cr[idx]=double(ycbcr.cr)-128.;
            }}
            // Usar las tablas DQT finales seleccionadas/escaladas
            JpegEncoder::encode_jpeg_block(block_Y, dqt_luma_final, prev_dc_Y, true, jpeg_data, mcu_current_byte, mcu_bits_in_current_byte);
            JpegEncoder::encode_jpeg_block(block_Cb, dqt_chroma_final, prev_dc_Cb, false, jpeg_data, mcu_current_byte, mcu_bits_in_current_byte);
            JpegEncoder::encode_jpeg_block(block_Cr, dqt_chroma_final, prev_dc_Cr, false, jpeg_data, mcu_current_byte, mcu_bits_in_current_byte);
        }
    }
    
    if (mcu_bits_in_current_byte > 0) { /* ... Padding final ... */ 
        mcu_current_byte <<= (8 - mcu_bits_in_current_byte); mcu_current_byte |= ((1 << (8 - mcu_bits_in_current_byte)) - 1); append_byte(mcu_current_byte); if (mcu_current_byte == 0xFF) { append_byte(0x00); } }
    append_byte(0xFF); append_byte(0xD9); // EOI

    std::ofstream archivo("image.jpg", std::ios::binary); if (!archivo){std::cerr<<"E:JPG"<<std::endl; return 1;}
    archivo.write(reinterpret_cast<const char*>(jpeg_data.data()), jpeg_data.size()); archivo.close();
    std::cout << "JPG (Full Encode): size=" << jpeg_data.size() << ", MCUs=" << total_blocks << std::endl;
    return 0;
}

//Función básica de suma de prueba
int add(int a, int b) { return a + b; }

//Función principal
int main(int argc, char *argv[]) { // Añadir argc y argv
    int quality = -1; // Predeterminado: usar alta calidad interna

    // Analizar argumentos de línea de comandos
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-q" && i + 1 < argc) {
            try {
                quality = std::stoi(argv[i + 1]);
                if (quality < 1 || quality > 100) {
                    std::cerr << "Advertencia: Calidad (-q) debe estar entre 1 y 100. Usando predeterminada." << std::endl;
                    quality = -1; // Revertir a predeterminada si está fuera de rango
                }
            } catch (const std::invalid_argument& e) {
                std::cerr << "Advertencia: Argumento de calidad inválido. Usando predeterminada." << std::endl;
                quality = -1;
            } catch (const std::out_of_range& e) {
                std::cerr << "Advertencia: Valor de calidad fuera de rango. Usando predeterminada." << std::endl;
                quality = -1;
            }
            i++; // Saltar el valor del argumento ya procesado
        } else if (arg == "-bmp") {
             if (write_bmp() != 0) return 1;
             // Podrías decidir salir aquí o continuar para generar también el JPG
             // return 0; // Salir después de generar solo BMP
        } else {
            std::cerr << "Argumento desconocido: " << arg << std::endl;
             std::cerr << "Uso: " << argv[0] << " [-q <1-100>] [-bmp]" << std::endl;
            return 1;
        }
    }

    // Llamar a write_jpg con la calidad especificada o predeterminada
    if (write_jpg(quality) != 0) {
        return 1;
    }
    
    // Opcional: Llamar a write_bmp si no se especificó -bmp y quieres ambos por defecto
    // if (argc == 1) { // Si no hay argumentos, genera ambos?
    //    if (write_bmp() != 0) return 1;
    // }


    std::cout << "Generación de imagen(es) completada." << std::endl;
    return 0;
}