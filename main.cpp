//Este es el script principal para mi motor gráfico
#include <iostream>
#include <string>
#include <cstdint>
#include <algorithm>
#include <fstream>
//Función para escribir al binario de un bitmap
//Como funciona?: Se escriber headers a un binario para hacer que sea un archivo bmp compatible
//hay 54 bytes en el header de un bitmap, entonces usamos uint8_t[] (unsigned integer 8bit type)
//y lo que hay en los corchetes es la cantidad de esos bytes que quieres asignar a la variable
//el número que pongas dentro de los corchetes es el número de bytes que quieres que se alloquen
//en la ram para guardar tus valores y todo lo que quieras.
//Hay dos tipos de haders que necesitamos poner en un bmp para que sea válido, el esencial es el
// file header, son 14 bytes y siguen la siguiente estructura:
//*-------------------------------------------------------------------------------*
//|Byte 0-1 : "B" "M" Firma BMP
//|Byte 2-5 : Número de bytes que ocupa el archivo
//|Byte 6-9 : Reservado, siempre es 0
//|Byte 10-13 : Offset en donde empieza el RGB de la imagen, siempre es 54 si la 
//|             imagen tiene colores de 24 bits y no tiene compresión que es en nuestro caso,
//|             igualmente la compresion en BMP esta en casi total deshuso
//*-------------------------------------------------------------------------------*
//Ahora vienen los Headers DIB , estos en vez de describir el archivo describen las características
//de la imagen en si, como la altura... hay edge cases con software antiguo como con Windows NT4 en donde son
//completamente distintos pero el que usamos hoy en día es el de Windows 3.0.
//*-------------------------------------------------------------------------------*
//|Byte 14-17 : Tamaño en bytes de lo que ocupa el header de DIB
//|Byte 18-21 : Ancho de la imagen en PX
//|Byte 22-25 : Alto de la imagen en PX
//|Byte 26-27 : Número de planos (Arcaico, siempre 1)
//|Byte 28-29 : Bits por pixel, 24 bits (3 bytes) es el estándar que usamos nosotros todo lo demas es edge case
//|Byte 30-33 : Compresión, casi siempre 0, no se suele usar compresión en Bitmaps
//|Byte 34-37 : Tamaño de la imagen en bytes, !!! BMP requiere que el número de bytes por fila sea múltiplo de 4 (mas adelante) !!!
//|Byte 38-41 : Ignorable, 0 (Resolución / Metros se usa para impresión)
//|Byte 42-45 : Ignorable, 0 (Resolución / Metros se usa para impresión)
//|Byte 46-49 : Colores en paleta, 0 para lossless
//|Byte 50-53 : Colores importantes, 0 para lossless
//*------------------------------------------------------------------------------*
//Procedo a declarar todos los headers para hacer una imagen de 2x2 px
//Declaro un array de 8 bits por elemento con 54 elementos para todos los headers

int write_image() {

    uint8_t header[54];

    //Hago variables los elementos de la imagen como la resolución para poder cambiarlo mas adelante
    //, por ahora dejo las variables en const

    const int resX = 500;
    const int resY = 500;

    //Pongo esto para poder manejar mas adelante edge cases pero podrían usarse directamente

    const int color_depth = 3;

    //!!! Aqui hay un poco un lío, como dije antes BMP requiere que el número de Bytes por fila sea múltiplo de 4 pero no nos inventamos datos,
    //  esto solamente se usa para el Byte 34-37 en donde tienes que dar el tamaño de la imagen con esas condiciones, seguramente si esto estal
    //  mal calculado no pase absolutamente nada en la mayoría de los visores de imágenes pero mas vale prevenir que curar, matemáticamente para
    //  redondear a un múltiplo de 4 es X = ((n + 3) / 4) x 4!!!

    const int image_size = ((((resX * color_depth) + 3) / 4) * 4) * resY;

    //Tamaño total del archivo, atención que aquí se sigue haciendo el redondeo de antes, uso headers + tamaño de la imagen que es mas rápido

    const int file_size = 54 + image_size;

    //Ahora escribimos todos estos datos que hemos calculado al header que vamos a escribir al archivo, los header que son siempre igual los pongo directamente

    header[0] = 'B';
    header[1] = 'M';
    header[2] = file_size & 0xFF;
    header[3] = (file_size >> 8) & 0xFF;
    header[4] = (file_size >> 16) & 0xFF;
    header[5] = (file_size >> 24) & 0xFF;
    //Uso una función de la libreria algorithm para rellenar del 6-9 pero se puede hacer manualmente nada lo impide
    std::fill(header + 6, header + 10, 0);
    //Offset, suele ser 54 bytes
    header[10] = 54;
    std::fill(header + 11, header + 14, 0);

    //DIB
    
    header[14] = 40;
    std::fill(header + 15, header + 18, 0);
    header[18] = resX & 0xFF;
    header[19] = (resX >> 8) & 0xFF;
    header[20] = (resX >> 16) & 0xFF;
    header[21] = (resX >> 24) & 0xFF;
    header[22] = resY & 0xFF;
    header[23] = (resY >> 8) & 0xFF;
    header[24] = (resY >> 16) & 0xFF;
    header[25] = (resY >> 24) & 0xFF;
    header[26] = 1;
    header[27] = 0;
    header[28] = 24;
    header[29] = 0;
    std::fill(header + 30, header + 34, 0);
    header[34] = image_size & 0xFF;
    header[35] = (image_size >> 8) & 0xFF;
    header[36] = (image_size >> 16) & 0xFF;
    header[37] = (image_size >> 24) & 0xFF;
    std::fill(header + 38, header + 42, 0);
    std::fill(header + 42, header + 46, 0);
    //Relleno esto directamente para los dos
    std::fill(header + 46, header + 54, 0);
    //Lo escribimos al archivo
    std::ofstream archivo("imagen.bmp", std::ios::binary);
    archivo.write(reinterpret_cast<char*>(header), 54);
    int padding = (((resX * color_depth) + 3) / 4) * 4 - (resX * color_depth);
    for (int y = 0; y < resY; ++y) {
        for (int x = 0; x < resX; ++x) {
            archivo.put(0);   // B
            archivo.put(0);   // G
            archivo.put(255); // R
        }
        for (int i = 0; i < padding; ++i)
            archivo.put(0); // relleno (padding)
    }
    archivo.close();
    std::cout << "file_size: " << file_size << ", image_size: " << image_size << ", padding: " << padding << std::endl;
    return 0;
}


//Función básica de suma de prueba
int add(int a, int b) {
    return a + b;
}

//Función principal
int main() {
    std::string x;
    x = std::to_string(add (12,6));
    std::cout << x << std::endl;
    write_image();
}