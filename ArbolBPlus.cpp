/**
 * Archivo: ArbolBPlus.cpp
 * Propósito: Aquí se implementan los métodos declarados en ArbolBPlus.h.
 *            Esta es la clase central que los estudiantes deberán completar
 *            como parte de su reto de Estructuras de Datos.
 */
#include "ArbolBPlus.h"
#include <cstdio> // std::remove, para borrar el archivo de persistencia en DROP TABLE

// -----------------------------------------------------------------------------
// Implementaciones de Estructuras Auxiliares
// -----------------------------------------------------------------------------

string Registro::serializar() const {
    // Ejemplo de salida: "1,Juan Perez" (clave + delimitador + datos)
    return to_string(clave) + "," + datos;
}

NodoBPlus::NodoBPlus(bool hoja) {
    // Por defecto al nacer, sabemos si es hoja o interno, pero no tiene nodos adyacentes aún.
    es_hoja = hoja;
    siguiente_hoja = nullptr;
}

// -----------------------------------------------------------------------------
// Constructor del Arbol B+
// -----------------------------------------------------------------------------

ArbolBPlus::ArbolBPlus(int _grado, string _nombre_archivo) : raiz(nullptr), grado(_grado), nombre_archivo(_nombre_archivo) {}

int ArbolBPlus::getGrado() const {
    return grado;
}

// =========================================================================
// INSERCIÓN (con split y propagación ascendente)
// =========================================================================

void ArbolBPlus::insertar(int clave, string datos) {
    // 1. Árbol vacío: se crea la primera hoja, que también es la raíz.
    if (raiz == nullptr) {
        raiz = new NodoBPlus(true);
        raiz->claves.push_back(clave);
        raiz->registros.push_back({clave, datos});
        return;
    }

    // 2. Descenso desde la raíz hasta la hoja correspondiente.
    NodoBPlus* cursor = raiz;
    while (!cursor->es_hoja) {
        size_t i;
        for (i = 0; i < cursor->claves.size(); i++) {
            if (clave < cursor->claves[i]) break;
        }
        cursor = cursor->hijos[i];
    }

    // 3. Inserción ordenada dentro de la hoja (o actualización si la clave ya existe).
    size_t pos = 0;
    while (pos < cursor->claves.size() && cursor->claves[pos] < clave) pos++;

    if (pos < cursor->claves.size() && cursor->claves[pos] == clave) {
        // La clave ya existe: se actualiza el dato (comportamiento tipo UPSERT).
        cursor->registros[pos].datos = datos;
        return;
    }

    cursor->claves.insert(cursor->claves.begin() + pos, clave);
    cursor->registros.insert(cursor->registros.begin() + pos, {clave, datos});

    // 4. Verificación de desbordamiento (la hoja excedió el grado permitido).
    if ((int)cursor->claves.size() <= grado) return; // No hubo overflow, terminamos.

    // 5. Split de la hoja: la mitad de los registros pasan a una hoja nueva.
    NodoBPlus* nuevaHoja = new NodoBPlus(true);
    int mitad = (int)cursor->claves.size() / 2;

    nuevaHoja->claves.assign(cursor->claves.begin() + mitad, cursor->claves.end());
    nuevaHoja->registros.assign(cursor->registros.begin() + mitad, cursor->registros.end());

    cursor->claves.resize(mitad);
    cursor->registros.resize(mitad);

    // Mantener la lista enlazada de hojas contigua.
    nuevaHoja->siguiente_hoja = cursor->siguiente_hoja;
    cursor->siguiente_hoja = nuevaHoja;

    int claveAscendida = nuevaHoja->claves.front();

    if (cursor == raiz) {
        // La hoja dividida era la raíz: se crea una nueva raíz interna.
        NodoBPlus* nuevaRaiz = new NodoBPlus(false);
        nuevaRaiz->claves.push_back(claveAscendida);
        nuevaRaiz->hijos.push_back(cursor);
        nuevaRaiz->hijos.push_back(nuevaHoja);
        raiz = nuevaRaiz;
        return;
    }

    NodoBPlus* padre = buscarPadre(raiz, cursor);
    insertarInterno(claveAscendida, padre, nuevaHoja);
}

// Inserta una clave promovida junto con el puntero al nuevo hijo dentro de un nodo interno,
// dividiendo dicho nodo interno (y propagando hacia arriba) si es necesario.
void ArbolBPlus::insertarInterno(int clave, NodoBPlus* cursor, NodoBPlus* hijo) {
    // Si el nodo interno todavía tiene espacio, simplemente insertamos.
    if ((int)cursor->claves.size() < grado) {
        size_t pos = 0;
        while (pos < cursor->claves.size() && cursor->claves[pos] < clave) pos++;
        cursor->claves.insert(cursor->claves.begin() + pos, clave);
        cursor->hijos.insert(cursor->hijos.begin() + pos + 1, hijo);
        return;
    }

    // No hay espacio: construimos un arreglo temporal con la clave/hijo nuevos incluidos
    // para luego dividir el nodo interno en dos.
    vector<int> tempClaves = cursor->claves;
    vector<NodoBPlus*> tempHijos = cursor->hijos;

    size_t pos = 0;
    while (pos < tempClaves.size() && tempClaves[pos] < clave) pos++;
    tempClaves.insert(tempClaves.begin() + pos, clave);
    tempHijos.insert(tempHijos.begin() + pos + 1, hijo);

    NodoBPlus* nuevoInterno = new NodoBPlus(false);
    size_t mitad = tempClaves.size() / 2;
    int claveAscendida = tempClaves[mitad];

    cursor->claves.assign(tempClaves.begin(), tempClaves.begin() + mitad);
    cursor->hijos.assign(tempHijos.begin(), tempHijos.begin() + mitad + 1);

    nuevoInterno->claves.assign(tempClaves.begin() + mitad + 1, tempClaves.end());
    nuevoInterno->hijos.assign(tempHijos.begin() + mitad + 1, tempHijos.end());

    if (cursor == raiz) {
        NodoBPlus* nuevaRaiz = new NodoBPlus(false);
        nuevaRaiz->claves.push_back(claveAscendida);
        nuevaRaiz->hijos.push_back(cursor);
        nuevaRaiz->hijos.push_back(nuevoInterno);
        raiz = nuevaRaiz;
        return;
    }

    NodoBPlus* padre = buscarPadre(raiz, cursor);
    insertarInterno(claveAscendida, padre, nuevoInterno);
}

// Busca el nodo padre de 'hijo' recorriendo el árbol desde 'cursor'.
NodoBPlus* ArbolBPlus::buscarPadre(NodoBPlus* cursor, NodoBPlus* hijo) {
    if (cursor == nullptr || cursor->es_hoja) return nullptr;

    for (NodoBPlus* h : cursor->hijos) {
        if (h == hijo) return cursor;
    }
    for (NodoBPlus* h : cursor->hijos) {
        if (!h->es_hoja) {
            NodoBPlus* resultado = buscarPadre(h, hijo);
            if (resultado != nullptr) return resultado;
        }
    }
    return nullptr;
}

// =========================================================================
// BÚSQUEDA (descenso logarítmico desde la raíz)
// =========================================================================

string ArbolBPlus::buscar(int clave) {
    if (raiz == nullptr) return "";

    NodoBPlus* cursor = raiz;
    while (!cursor->es_hoja) {
        size_t i;
        for (i = 0; i < cursor->claves.size(); i++) {
            if (clave < cursor->claves[i]) break;
        }
        cursor = cursor->hijos[i];
    }

    for (size_t i = 0; i < cursor->claves.size(); i++) {
        if (cursor->claves[i] == clave) return cursor->registros[i].datos;
    }
    return "";
}

// =========================================================================
// ELIMINACIÓN (bono: underflow con préstamo entre hermanos y fusión/merge)
// =========================================================================

int ArbolBPlus::indiceHijo(NodoBPlus* padre, NodoBPlus* hijo) {
    for (size_t i = 0; i < padre->hijos.size(); i++) {
        if (padre->hijos[i] == hijo) return (int)i;
    }
    return -1;
}

void ArbolBPlus::eliminar(int clave) {
    if (raiz == nullptr) {
        cout << "[Arbol B+] El arbol esta vacio.\n";
        return;
    }

    // Descenso guardando el camino recorrido para poder llegar al padre de la hoja.
    NodoBPlus* cursor = raiz;
    while (!cursor->es_hoja) {
        size_t i;
        for (i = 0; i < cursor->claves.size(); i++) {
            if (clave < cursor->claves[i]) break;
        }
        cursor = cursor->hijos[i];
    }

    int pos = -1;
    for (size_t i = 0; i < cursor->claves.size(); i++) {
        if (cursor->claves[i] == clave) { pos = (int)i; break; }
    }
    if (pos == -1) {
        cout << "[Arbol B+] Clave " << clave << " no encontrada.\n";
        return;
    }

    cursor->claves.erase(cursor->claves.begin() + pos);
    cursor->registros.erase(cursor->registros.begin() + pos);

    if (cursor == raiz) {
        // La raíz es una hoja: no aplican reglas de underflow.
        if (cursor->claves.empty()) {
            delete raiz;
            raiz = nullptr;
        }
        return;
    }

    int minHoja = (grado + 1) / 2; // Mínimo de claves permitido en una hoja no-raíz.
    if ((int)cursor->claves.size() >= minHoja) return; // No hay underflow.

    NodoBPlus* padre = buscarPadre(raiz, cursor);
    int idx = indiceHijo(padre, cursor);
    NodoBPlus* izq = (idx > 0) ? padre->hijos[idx - 1] : nullptr;
    NodoBPlus* der = (idx < (int)padre->hijos.size() - 1) ? padre->hijos[idx + 1] : nullptr;

    // Intento de préstamo (redistribución) desde el hermano izquierdo.
    if (izq != nullptr && (int)izq->claves.size() > minHoja) {
        cursor->claves.insert(cursor->claves.begin(), izq->claves.back());
        cursor->registros.insert(cursor->registros.begin(), izq->registros.back());
        izq->claves.pop_back();
        izq->registros.pop_back();
        padre->claves[idx - 1] = cursor->claves.front();
        return;
    }
    // Intento de préstamo desde el hermano derecho.
    if (der != nullptr && (int)der->claves.size() > minHoja) {
        cursor->claves.push_back(der->claves.front());
        cursor->registros.push_back(der->registros.front());
        der->claves.erase(der->claves.begin());
        der->registros.erase(der->registros.begin());
        padre->claves[idx] = der->claves.front();
        return;
    }

    // No fue posible redistribuir: se realiza fusión (merge) con un hermano.
    if (izq != nullptr) {
        for (size_t i = 0; i < cursor->claves.size(); i++) {
            izq->claves.push_back(cursor->claves[i]);
            izq->registros.push_back(cursor->registros[i]);
        }
        izq->siguiente_hoja = cursor->siguiente_hoja;
        padre->claves.erase(padre->claves.begin() + (idx - 1));
        padre->hijos.erase(padre->hijos.begin() + idx);
        delete cursor;
        eliminarInterno(padre);
    } else if (der != nullptr) {
        for (size_t i = 0; i < der->claves.size(); i++) {
            cursor->claves.push_back(der->claves[i]);
            cursor->registros.push_back(der->registros[i]);
        }
        cursor->siguiente_hoja = der->siguiente_hoja;
        padre->claves.erase(padre->claves.begin() + idx);
        padre->hijos.erase(padre->hijos.begin() + (idx + 1));
        delete der;
        eliminarInterno(padre);
    }
}

// Corrige el underflow de un nodo interno tras una fusión de sus hijos, propagando
// el préstamo/fusión hacia arriba si es necesario (hasta la raíz).
void ArbolBPlus::eliminarInterno(NodoBPlus* nodo) {
    if (nodo == raiz) {
        // Si la raíz quedó con un solo hijo, ese hijo se convierte en la nueva raíz.
        if (nodo->hijos.size() == 1) {
            raiz = nodo->hijos[0];
            delete nodo;
        }
        return;
    }

    int minHijos = (grado + 2) / 2; // Mínimo de hijos permitido en un nodo interno no-raíz.
    if ((int)nodo->hijos.size() >= minHijos) return;

    NodoBPlus* padre = buscarPadre(raiz, nodo);
    int idx = indiceHijo(padre, nodo);
    NodoBPlus* izq = (idx > 0) ? padre->hijos[idx - 1] : nullptr;
    NodoBPlus* der = (idx < (int)padre->hijos.size() - 1) ? padre->hijos[idx + 1] : nullptr;

    // Préstamo desde el hermano interno izquierdo.
    if (izq != nullptr && (int)izq->hijos.size() > minHijos) {
        nodo->claves.insert(nodo->claves.begin(), padre->claves[idx - 1]);
        nodo->hijos.insert(nodo->hijos.begin(), izq->hijos.back());
        padre->claves[idx - 1] = izq->claves.back();
        izq->claves.pop_back();
        izq->hijos.pop_back();
        return;
    }
    // Préstamo desde el hermano interno derecho.
    if (der != nullptr && (int)der->hijos.size() > minHijos) {
        nodo->claves.push_back(padre->claves[idx]);
        nodo->hijos.push_back(der->hijos.front());
        padre->claves[idx] = der->claves.front();
        der->claves.erase(der->claves.begin());
        der->hijos.erase(der->hijos.begin());
        return;
    }

    // Fusión con un hermano interno, arrastrando la clave separadora del padre.
    if (izq != nullptr) {
        izq->claves.push_back(padre->claves[idx - 1]);
        for (int c : nodo->claves) izq->claves.push_back(c);
        for (NodoBPlus* h : nodo->hijos) izq->hijos.push_back(h);
        padre->claves.erase(padre->claves.begin() + (idx - 1));
        padre->hijos.erase(padre->hijos.begin() + idx);
        delete nodo;
        eliminarInterno(padre);
    } else if (der != nullptr) {
        nodo->claves.push_back(padre->claves[idx]);
        for (int c : der->claves) nodo->claves.push_back(c);
        for (NodoBPlus* h : der->hijos) nodo->hijos.push_back(h);
        padre->claves.erase(padre->claves.begin() + idx);
        padre->hijos.erase(padre->hijos.begin() + (idx + 1));
        delete der;
        eliminarInterno(padre);
    }
}

// =========================================================================
// RECORRIDO SECUENCIAL (Full Table Scan usando la lista enlazada de hojas)
// =========================================================================

vector<Registro> ArbolBPlus::obtenerTodos() {
    vector<Registro> resultado;
    if (raiz == nullptr) return resultado;

    // 1. Descender siempre por el hijo más a la izquierda hasta la primera hoja.
    NodoBPlus* cursor = raiz;
    while (!cursor->es_hoja) {
        cursor = cursor->hijos[0];
    }

    // 2. Recorrer todas las hojas siguiendo "siguiente_hoja".
    while (cursor != nullptr) {
        for (const Registro& r : cursor->registros) {
            resultado.push_back(r);
        }
        cursor = cursor->siguiente_hoja;
    }
    return resultado;
}

// =========================================================================
// LIBERACIÓN DE MEMORIA / DROP TABLE
// =========================================================================

void ArbolBPlus::liberarNodo(NodoBPlus* nodo) {
    if (nodo == nullptr) return;
    if (!nodo->es_hoja) {
        for (NodoBPlus* h : nodo->hijos) liberarNodo(h);
    }
    delete nodo;
}

void ArbolBPlus::vaciar() {
    liberarNodo(raiz);
    raiz = nullptr;
    remove(nombre_archivo.c_str()); // Elimina el archivo de persistencia asociado.
}

// =========================================================================
// MÉTODOS DE PERSISTENCIA (ARCHIVO DE TEXTO)
// =========================================================================

void ArbolBPlus::guardarEnArchivo() {
    ofstream archivo(nombre_archivo);

    // Si no tenemos permisos o la ruta falla, abortamos
    if (!archivo.is_open()) {
        cerr << "Error al abrir el archivo " << nombre_archivo << endl;
        return;
    }

    // Recorremos todas las hojas (recorrido secuencial) y escribimos cada registro serializado.
    vector<Registro> todos = obtenerTodos();
    for (const Registro& r : todos) {
        archivo << r.serializar() << "\n";
    }

    archivo.close();
}

void ArbolBPlus::cargarDesdeArchivo() {
    ifstream archivo(nombre_archivo);

    // Si el archivo no existe (ej. es la primera vez que corre el programa), ignorar sin error grave.
    if (!archivo.is_open()) {
        cout << "No existe archivo previo '" << nombre_archivo << "'. Se creará al guardar.\n";
        return;
    }

    string linea;
    while (getline(archivo, linea)) {
        if (linea.empty()) continue;
        size_t posComa = linea.find(',');
        if (posComa == string::npos) continue; // línea corrupta/incompleta, se ignora

        int clave = stoi(linea.substr(0, posComa));
        string datos = linea.substr(posComa + 1);
        insertar(clave, datos);
    }

    archivo.close();
}
