//Este es el script principal para mi motor gráfico
#include <iostream>
#include <string>
#include <cstdint>

//Función para escribir al binario de un bitmap
//Como funciona?: Se escriber headers a un binario para hacer que sea un archivo bmp compatible
//hay 54 bytes en el header de un bitmap, entonces usamos uint8_t[] (unsigned integer 8bit type)
//y lo que hay en los corchetes es la cantidad de esos bytes que quieres asignar a la variable
//el número que pongas dentro de los corchetes es el número de bytes que quieres que se alloquen
//en la ram para guardar tus valores y todo lo que quieras.
//Hay dos tipos de haders que necesitamos poner en un bmp para que sea válido, el esencial es el
// file header, son 14 bytes y siguen la siguiente estructura:
//Byte 0-1 : "B" "M" Firma BMP
//Byte 2-5 : Número de bytes que ocupa el archivo
//Byte 6-9 : Reservado, siempre es 0
//Byte 10-13 : Offset en donde empieza la imagen, siempre suele ser 54
//es el byte en el que empieza el contenido de la imagen en si
int write_image() {

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
}