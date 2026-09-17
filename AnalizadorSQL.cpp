/**
 * Archivo: AnalizadorSQL.cpp
 * Propósito: Implementación del Analizador Léxico y Sintáctico rudimentario.
 *            Este archivo procesa los strings ingresados por el usuario,
 *            extrae los tokens (como el ID o los datos a guardar) y llama a
 *            las funciones del árbol (ArbolBPlus.h).
 */
#include "AnalizadorSQL.h"

// Inicializa el analizador acoplando el puntero del árbol B+ que se pasó en el main
AnalizadorSQL::AnalizadorSQL(ArbolBPlus* base_datos) : bd(base_datos), nombreTabla("") {}

AnalizadorSQL::~AnalizadorSQL() {
    // Los índices secundarios se crean dinámicamente en tiempo de ejecución (CREATE INDEX),
    // así que hay que liberarlos para no dejar memoria huérfana.
    for (IndiceInfo& idx : indices) {
        delete idx.arbol;
    }
}

string AnalizadorSQL::aMayusculas(string cadena) {
    string cadenaMayus = "";
    // Itera letra por letra convirtiendo a su equivalente en mayúscula
    for (char c : cadena) cadenaMayus += toupper(c);
    return cadenaMayus;
}

// ---------------------------------------------------------------------------
// Auxiliares de parseo
// ---------------------------------------------------------------------------

string AnalizadorSQL::recortar(const string& cadena) {
    size_t inicio = cadena.find_first_not_of(" \t\r\n");
    if (inicio == string::npos) return "";
    size_t fin = cadena.find_last_not_of(" \t\r\n");
    return cadena.substr(inicio, fin - inicio + 1);
}

string AnalizadorSQL::quitarComillas(const string& cadena) {
    string limpio = recortar(cadena);
    if (limpio.size() >= 2) {
        char primero = limpio.front();
        char ultimo = limpio.back();
        if ((primero == '\'' && ultimo == '\'') || (primero == '"' && ultimo == '"')) {
            return limpio.substr(1, limpio.size() - 2);
        }
    }
    return limpio;
}

// Divide una cadena por comas de nivel superior (suficiente para listas simples como
// "id INT, nombre STR" o valores de columnas sin comillas anidadas complejas).
vector<string> AnalizadorSQL::dividirPorComas(const string& cadena) {
    vector<string> partes;
    string actual;
    for (char c : cadena) {
        if (c == ',') {
            partes.push_back(recortar(actual));
            actual.clear();
        } else {
            actual += c;
        }
    }
    partes.push_back(recortar(actual));
    return partes;
}

// Extrae el nombre de la tabla que sigue a la palabra clave FROM o INTO.
string AnalizadorSQL::extraerNombreTabla(const string& consulta, const string& consultaMayus) {
    size_t pos = consultaMayus.find("FROM");
    size_t largoPalabra = 4;
    if (pos == string::npos) {
        pos = consultaMayus.find("INTO");
        largoPalabra = 4;
    }
    if (pos == string::npos) return "";

    size_t inicio = pos + largoPalabra;
    while (inicio < consulta.size() && consulta[inicio] == ' ') inicio++;
    size_t fin = inicio;
    while (fin < consulta.size() && consulta[fin] != ' ' && consulta[fin] != '(') fin++;
    return recortar(consulta.substr(inicio, fin - inicio));
}

bool AnalizadorSQL::validarTabla(const string& nombreTablaConsulta) {
    if (nombreTabla.empty()) {
        cout << "Error: no existe ninguna tabla creada. Use CREATE TABLE primero.\n";
        return false;
    }
    if (aMayusculas(nombreTablaConsulta) != aMayusculas(nombreTabla)) {
        cout << "Error: la tabla '" << nombreTablaConsulta << "' no existe. "
             << "La tabla activa es '" << nombreTabla << "'.\n";
        return false;
    }
    return true;
}

// Hash polinómico simple para convertir el valor de una columna (string) en una clave
// entera utilizable por el Árbol B+ del índice secundario.
int AnalizadorSQL::hashCadena(const string& cadena) {
    long long resultado = 0;
    for (char c : cadena) {
        resultado = (resultado * 131 + (unsigned char)c) % 1000000007LL;
    }
    return (int)resultado;
}

// Ante cada INSERT, propaga el nuevo registro hacia todos los índices secundarios activos.
void AnalizadorSQL::sincronizarIndices(int id, const string& datos) {
    if (indices.empty()) return;

    vector<string> columnasValor = dividirPorComas(datos);

    for (IndiceInfo& idx : indices) {
        int claveIndice;
        if (idx.posicionColumna == -1) {
            // Índice sobre la propia columna "id": la clave del índice es el id mismo.
            claveIndice = id;
        } else if (idx.posicionColumna >= 0 && idx.posicionColumna < (int)columnasValor.size()) {
            claveIndice = hashCadena(quitarComillas(columnasValor[idx.posicionColumna]));
        } else {
            continue; // La columna indexada no aparece en este registro; se omite.
        }
        idx.arbol->insertar(claveIndice, to_string(id));
    }
}

// ---------------------------------------------------------------------------
// Punto de entrada
// ---------------------------------------------------------------------------

void AnalizadorSQL::ejecutarConsulta(string consulta) {
    if (consulta.empty()) return;

    // Stringstream permite leer palabras de un string separadas por espacios
    stringstream ss(consulta);
    string comando;

    // Extrae la primera palabra (Ej. "INSERT", "SELECT")
    ss >> comando;
    comando = aMayusculas(comando); // Normalizamos a mayúsculas para las comparaciones

    // Derivación según familia de instrucciones SQL
    if (comando == "CREATE" || comando == "DROP") {
        analizarDDL(consulta, comando);
    } else if (comando == "SELECT" || comando == "INSERT" || comando == "DELETE") {
        analizarDQL_DML(consulta, comando);
    } else if (comando == "HELP") {
        mostrarAyuda();
    } else {
        cout << "Error: Comando SQL no reconocido. Escriba HELP para mas informacion.\n";
    }
}

// ---------------------------------------------------------------------------
// DDL: CREATE TABLE / CREATE INDEX / DROP TABLE
// ---------------------------------------------------------------------------

void AnalizadorSQL::analizarDDL(string consulta, string comando) {
    string consultaMayus = aMayusculas(consulta);

    if (comando == "CREATE") {
        if (consultaMayus.find("INDEX") != string::npos) {
            // Ejemplo: CREATE INDEX idx_nombre ON usuarios (nombre)
            size_t posIndex = consultaMayus.find("INDEX");
            size_t posOn = consultaMayus.find("ON");
            size_t posParenIni = consulta.find('(', posOn);
            size_t posParenFin = consulta.find(')', posParenIni);

            if (posOn == string::npos || posParenIni == string::npos || posParenFin == string::npos) {
                cout << "Error de sintaxis en CREATE INDEX. Use: CREATE INDEX <nombre> ON <tabla> (<columna>)\n";
                return;
            }

            string nombreIndice = recortar(consulta.substr(posIndex + 5, posOn - (posIndex + 5)));
            string tablaObjetivo = recortar(consulta.substr(posOn + 2, posParenIni - (posOn + 2)));
            string columna = recortar(consulta.substr(posParenIni + 1, posParenFin - posParenIni - 1));

            if (!validarTabla(tablaObjetivo)) return;

            int posicionColumna = -1; // -1 significa "es la columna id"
            if (aMayusculas(columna) != "ID") {
                posicionColumna = -2; // no encontrada por defecto
                for (size_t i = 0; i < columnasTabla.size(); i++) {
                    if (aMayusculas(columnasTabla[i]) == aMayusculas(columna)) {
                        posicionColumna = (int)i;
                        break;
                    }
                }
                if (posicionColumna == -2) {
                    cout << "Error: la columna '" << columna << "' no existe en la tabla '" << tablaObjetivo << "'.\n";
                    return;
                }
            }

            ArbolBPlus* arbolIndice = new ArbolBPlus(bd->getGrado(), nombreIndice + "_indice.txt");
            indices.push_back({nombreIndice, columna, posicionColumna, arbolIndice});

            // Poblamos el índice con los registros que ya existieran en la tabla.
            vector<Registro> existentes = bd->obtenerTodos();
            for (const Registro& r : existentes) {
                sincronizarIndices(r.clave, r.datos);
            }

            cout << "[Ejecutando DDL] -> Indice secundario '" << nombreIndice
                 << "' creado sobre la columna '" << columna << "' de '" << tablaObjetivo << "'.\n";
        } else {
            // Ejemplo: CREATE TABLE usuarios (id INT, nombre STR)
            size_t posTable = consultaMayus.find("TABLE");
            size_t posParenIni = consulta.find('(');
            size_t posParenFin = consulta.rfind(')');

            if (posTable == string::npos || posParenIni == string::npos || posParenFin == string::npos) {
                cout << "Error de sintaxis en CREATE TABLE. Use: CREATE TABLE <nombre> (columnas...)\n";
                return;
            }

            string nombre = recortar(consulta.substr(posTable + 5, posParenIni - (posTable + 5)));
            string columnasCrudo = consulta.substr(posParenIni + 1, posParenFin - posParenIni - 1);
            vector<string> definiciones = dividirPorComas(columnasCrudo);

            nombreTabla = nombre;
            columnasTabla.clear();
            // La primera columna se asume como el "id" (clave primaria); el resto son
            // las columnas de datos, en el orden en que aparecerán dentro de VALUES.
            for (size_t i = 1; i < definiciones.size(); i++) {
                stringstream defs(definiciones[i]);
                string nombreColumna;
                defs >> nombreColumna; // primer token de "nombre TIPO"
                columnasTabla.push_back(nombreColumna);
            }

            cout << "[Ejecutando DDL] -> Tabla '" << nombreTabla << "' creada con "
                 << columnasTabla.size() << " columna(s) de datos (ademas de id).\n";
        }
    } else if (comando == "DROP") {
        // Ejemplo: DROP TABLE usuarios
        size_t posTable = consultaMayus.find("TABLE");
        if (posTable == string::npos) {
            cout << "Error de sintaxis en DROP TABLE. Use: DROP TABLE <nombre>\n";
            return;
        }
        string nombre = recortar(consulta.substr(posTable + 5));
        if (!validarTabla(nombre)) return;

        bd->vaciar();
        for (IndiceInfo& idx : indices) {
            idx.arbol->vaciar();
            delete idx.arbol;
        }
        indices.clear();
        columnasTabla.clear();

        cout << "[Ejecutando DDL] -> Tabla '" << nombreTabla << "' eliminada (memoria y archivo liberados).\n";
        nombreTabla = "";
    }
}

// ---------------------------------------------------------------------------
// DML / DQL: INSERT / SELECT / DELETE
// ---------------------------------------------------------------------------

void AnalizadorSQL::analizarDQL_DML(string consulta, string comando) {
    string consultaMayus = aMayusculas(consulta);

    if (comando == "INSERT") {
        // Ejemplo: INSERT INTO usuarios VALUES (10, 'Juan Perez, 25')
        string tabla = extraerNombreTabla(consulta, consultaMayus);
        if (!validarTabla(tabla)) return;

        size_t posValues = consultaMayus.find("VALUES");
        size_t posParenIni = consulta.find('(', posValues);
        size_t posParenFin = consulta.rfind(')');

        if (posValues == string::npos || posParenIni == string::npos || posParenFin == string::npos) {
            cout << "Error de sintaxis en INSERT. Use: INSERT INTO <tabla> VALUES (<id>, <datos>)\n";
            return;
        }

        string contenido = consulta.substr(posParenIni + 1, posParenFin - posParenIni - 1);
        size_t posComa = contenido.find(',');
        if (posComa == string::npos) {
            cout << "Error de sintaxis en INSERT: se esperaba al menos (id, datos).\n";
            return;
        }

        int id;
        try {
            id = stoi(recortar(contenido.substr(0, posComa)));
        } catch (...) {
            cout << "Error: el id proporcionado no es un entero valido.\n";
            return;
        }
        string datos = quitarComillas(contenido.substr(posComa + 1));

        bd->insertar(id, datos);
        sincronizarIndices(id, datos);

        cout << "[Ejecutando DML] -> Registro insertado (id=" << id << ", datos=\"" << datos << "\").\n";
    }
    else if (comando == "SELECT") {
        // Ejemplo: SELECT * FROM tabla; o SELECT * FROM tabla WHERE id = 10;
        string tabla = extraerNombreTabla(consulta, consultaMayus);
        if (!validarTabla(tabla)) return;

        if (consultaMayus.find("WHERE") != string::npos) {
            size_t posIgual = consulta.find('=');
            if (posIgual == string::npos) {
                cout << "Error de sintaxis en SELECT WHERE. Use: SELECT * FROM <tabla> WHERE id = <id>\n";
                return;
            }
            int id;
            try {
                id = stoi(recortar(consulta.substr(posIgual + 1)));
            } catch (...) {
                cout << "Error: el id de busqueda no es un entero valido.\n";
                return;
            }

            string resultado = bd->buscar(id);
            if (resultado.empty()) {
                cout << "[Ejecutando DQL] -> No se encontro ningun registro con id=" << id << ".\n";
            } else {
                cout << "[Ejecutando DQL] -> id=" << id << " | datos=" << resultado << "\n";
            }
        } else {
            vector<Registro> resultados = bd->obtenerTodos();
            if (resultados.empty()) {
                cout << "[Ejecutando DQL] -> La tabla '" << nombreTabla << "' no tiene registros.\n";
            } else {
                cout << "[Ejecutando DQL] -> " << resultados.size() << " registro(s):\n";
                for (const Registro& r : resultados) {
                    cout << "  id=" << r.clave << " | datos=" << r.datos << "\n";
                }
            }
        }
    }
    else if (comando == "DELETE") {
        // Ejemplo: DELETE FROM tabla WHERE id = 10
        string tabla = extraerNombreTabla(consulta, consultaMayus);
        if (!validarTabla(tabla)) return;

        size_t posWhere = consultaMayus.find("WHERE");
        size_t posIgual = consulta.find('=');
        if (posWhere == string::npos || posIgual == string::npos) {
            cout << "Error de sintaxis en DELETE. Use: DELETE FROM <tabla> WHERE id = <id>\n";
            return;
        }

        int id;
        try {
            id = stoi(recortar(consulta.substr(posIgual + 1)));
        } catch (...) {
            cout << "Error: el id a eliminar no es un entero valido.\n";
            return;
        }

        bd->eliminar(id);
        cout << "[Ejecutando DML] -> Solicitud de eliminacion procesada para id=" << id << ".\n";
    }
}

void AnalizadorSQL::mostrarAyuda() {
    // Configuración de colores estándar ANSI para terminales modernas
    const string RESET = "\033[0m";
    const string BOLD_YELLOW = "\033[1;33m";
    const string BOLD_CYAN = "\033[1;36m";
    const string BOLD_GREEN = "\033[1;32m";
    const string BOLD_WHITE = "\033[1;37m";

    cout << BOLD_YELLOW << "\n=== Sistema Gestor SQL basado en Árboles B+ ===" << RESET << "\n";
    cout << BOLD_WHITE << "Comandos Soportados (Esqueleto):" << RESET << "\n";

    // Categoría DDL
    cout << BOLD_YELLOW << "  [DDL - Lenguaje de Definición de Datos]" << RESET << "\n";
    cout << BOLD_CYAN << "    Sintaxis: CREATE TABLE <nombre> (columnas...)" << RESET << "\n";
    cout << BOLD_GREEN << "    Ejemplo : CREATE TABLE usuarios (id INT, nombre STR)" << RESET << "\n\n";

    cout << BOLD_CYAN << "    Sintaxis: CREATE INDEX <nombre> ON <tabla> (columna)" << RESET << "\n";
    cout << BOLD_GREEN << "    Ejemplo : CREATE INDEX idx_nombre ON usuarios (nombre)" << RESET << "\n\n";

    cout << BOLD_CYAN << "    Sintaxis: DROP TABLE <nombre>" << RESET << "\n";
    cout << BOLD_GREEN << "    Ejemplo : DROP TABLE usuarios" << RESET << "\n\n";

    // Categoría DML/DQL
    cout << BOLD_YELLOW << "  [DQL / DML - Manipulación y Consulta]" << RESET << "\n";
    cout << BOLD_CYAN << "    Sintaxis: INSERT INTO <nombre> VALUES (<id>, <datos>)" << RESET << "\n";
    cout << BOLD_GREEN << "    Ejemplo : INSERT INTO usuarios VALUES (10, 'Juan Perez, 25')" << RESET << "\n\n";

    cout << BOLD_CYAN << "    Sintaxis: SELECT * FROM <nombre>" << RESET << "\n";
    cout << BOLD_GREEN << "    Ejemplo : SELECT * FROM usuarios" << RESET << "\n\n";

    cout << BOLD_CYAN << "    Sintaxis: SELECT * FROM <nombre> WHERE id = <id>" << RESET << "\n";
    cout << BOLD_GREEN << "    Ejemplo : SELECT * FROM usuarios WHERE id = 10" << RESET << "\n\n";

    cout << BOLD_CYAN << "    Sintaxis: DELETE FROM <nombre> WHERE id = <id>" << RESET << "\n";
    cout << BOLD_GREEN << "    Ejemplo : DELETE FROM usuarios WHERE id = 10" << RESET << "\n\n";

    // Controles Base
    cout << BOLD_YELLOW << "  [Otros Comandos]" << RESET << "\n";
    cout << BOLD_CYAN << "    HELP  - Muestra este menu" << RESET << "\n";
    cout << BOLD_CYAN << "    EXIT  - Guarda los datos y sale del programa" << RESET << "\n";
    cout << BOLD_YELLOW << "================================================" << RESET << "\n\n";
}
