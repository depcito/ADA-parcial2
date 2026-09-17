/**
 * Archivo: AnalizadorSQL.h
 * Propósito: Define la clase encargada de tomar cadenas de texto, identificar
 *            los comandos (INSERT, SELECT, CREATE) y delegar la carga de trabajo
 *            al motor de almacenamiento subyacente (Arbol B+).
 */
#ifndef ANALIZADOR_SQL_H
#define ANALIZADOR_SQL_H

#include <iostream>  // Salida estandar a consola
#include <string>    // Operaciones con cadenas
#include <sstream>   // 'stringstream' permite partir un texto en palabras facilmente
#include <vector>    // Uso de arrays dinamicos
#include "ArbolBPlus.h" // Se requiere para poder despachar los comandos al arbol

using namespace std;

// Metadatos de un índice secundario: un segundo Árbol B+ cuya clave es la columna
// indexada (convertida a entero) y cuyo dato apunta a la clave primaria original.
struct IndiceInfo {
    string nombre;       // Nombre del índice (ej. idx_nombre)
    string columna;       // Columna sobre la que se indexa (ej. "nombre")
    int posicionColumna;  // Posición de la columna dentro de 'datos' (-1 si la columna es "id")
    ArbolBPlus* arbol;    // Árbol B+ secundario propio de este índice
};

class AnalizadorSQL {
private:
    ArbolBPlus* bd; // Puntero a la base de datos instanciada en el main

    string nombreTabla;            // Nombre de la tabla actualmente creada ("" si no hay ninguna)
    vector<string> columnasTabla;  // Nombres de columnas (sin contar "id"), en el orden declarado
    vector<IndiceInfo> indices;    // Índices secundarios activos sobre la tabla

    // Convierte cualquier cadena a mayúsculas para evitar problemas de "Select" vs "SELECT"
    string aMayusculas(string cadena);

    // ------------------------------------------------------------------
    // Auxiliares de parseo de texto (usan find/substr, sin regex)
    // ------------------------------------------------------------------
    string recortar(const string& cadena);                       // Quita espacios en los bordes
    string quitarComillas(const string& cadena);                 // Quita comillas simples/dobles envolventes
    vector<string> dividirPorComas(const string& cadena);         // Split simple por comas de nivel superior
    string extraerNombreTabla(const string& consulta, const string& consultaMayus); // tras FROM/INTO
    bool validarTabla(const string& nombreTablaConsulta);         // Verifica que exista y coincida
    int hashCadena(const string& cadena);                        // Convierte un string en un entero (para índices)
    void sincronizarIndices(int id, const string& datos);        // Inserta el registro en todos los índices activos

public:
    // Constructor que recibe el árbol B+ y lo guarda como atributo
    AnalizadorSQL(ArbolBPlus* base_datos);

    // Libera los árboles de los índices secundarios creados en tiempo de ejecución
    ~AnalizadorSQL();

    /**
     * @brief Método de entrada del analizador. Recibe la consulta en crudo.
     * Separa la primera palabra de la consulta para derivar el flujo.
     */
    void ejecutarConsulta(string consulta);

    // ==========================================
    // PARSEO DE DDL (Lenguaje de Definición de Datos)
    // ==========================================
    // Comandos enfocados en la estructura global (Ej. CREATE TABLE, DROP TABLE)
    void analizarDDL(string consulta, string comando);

    // ==========================================
    // PARSEO DE DQL y DML (Consultas y Manipulación de Datos)
    // ==========================================
    // Comandos enfocados en la información interna (Ej. INSERT, SELECT, DELETE)
    void analizarDQL_DML(string consulta, string comando);

    // Muestra en consola las ayudas formatadas con colores ANSI
    void mostrarAyuda();
};

#endif // ANALIZADOR_SQL_H
