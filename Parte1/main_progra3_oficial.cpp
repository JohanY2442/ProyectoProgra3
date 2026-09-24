#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>
#include <cctype>
#include <cstdlib>
#include <limits>
#include <algorithm>

using namespace std;

// ---------------------------------------
// ESTRUCTURA DE PELÍCULA
// ---------------------------------------
struct Movie {
    int year = 0;
    string title;
    string origin;
    string director;
    string cast;
    string genre;
    string wiki_url;
    string plot;
};

// ---------------------------------------
// NODO DEL TRIE DE SUFIJOS
// ---------------------------------------
struct TrieNode {
    map<char, TrieNode*> children;
    vector<int> movieIndices;  // Índices de películas que contienen este sufijo
};

class SuffixTrie {
private:
    TrieNode* root;
    vector<Movie> movies;

public:
    SuffixTrie() {
        root = new TrieNode();
    }

    // Liberar toda la memoria del árbol al terminar
    ~SuffixTrie() {
        deleteNode(root);
    }

    // El árbol maneja memoria dinámica: no se permite copiarlo
    SuffixTrie(const SuffixTrie&) = delete;
    SuffixTrie& operator=(const SuffixTrie&) = delete;

    // Cargar películas desde CSV (devuelve false si no se pudo abrir)
    bool loadMovies(const string& filename) {
        ifstream file(filename);
        if (!file.is_open()) {
            return false;
        }

        string record;
        readCSVRecord(file, record);  // Saltar encabezado

        int movieCount = 0;
        while (readCSVRecord(file, record)) {  // Cargar TODAS las películas
            if (record.empty()) continue;      // Ignorar líneas vacías

            Movie movie = parseCSVLine(record);
            movies.push_back(movie);

            // Insertar solo sufijos del título (más eficiente)
            insertSuffixes(movie.title, movieCount);

            movieCount++;

            // Mostrar progreso cada 5000 películas
            if (movieCount % 5000 == 0) {
                cout << "Cargadas " << movieCount << " películas...\n";
            }
        }

        file.close();
        cout << "\n✓ Total de películas cargadas: " << movies.size() << endl;
        return true;
    }

    // Insertar todos los sufijos de un texto
    void insertSuffixes(const string& text, int movieIndex) {
        for (size_t i = 0; i < text.length(); i++) {
            insertSuffix(text.substr(i), movieIndex);
        }
    }

    // Insertar un sufijo en el Trie
    void insertSuffix(const string& suffix, int movieIndex) {
        TrieNode* current = root;
        for (char c : suffix) {
            if (current->children.find(c) == current->children.end()) {
                current->children[c] = new TrieNode();
            }
            current = current->children[c];

            // Añadir el índice de la película si no está ya.
            // Las películas se insertan en orden, así que basta revisar el último.
            if (current->movieIndices.empty() || current->movieIndices.back() != movieIndex) {
                current->movieIndices.push_back(movieIndex);
            }
        }
    }

    // Buscar todas las películas que contienen un sub-palabra
    vector<int> search(const string& query) {
        TrieNode* current = root;
        string lowerQuery = toLower(query);

        for (char c : lowerQuery) {
            if (current->children.find(c) == current->children.end()) {
                return {};  // No encontrado
            }
            current = current->children[c];
        }

        return current->movieIndices;
    }

    // Buscar por director
    vector<int> searchByDirector(const string& director) {
        vector<int> results;
        string lowerDirector = toLower(director);

        for (size_t i = 0; i < movies.size(); i++) {
            if (movies[i].director.find(lowerDirector) != string::npos) {
                results.push_back(i);
            }
        }

        return results;
    }

    // Buscar por género
    vector<int> searchByGenre(const string& genre) {
        vector<int> results;
        string lowerGenre = toLower(genre);

        for (size_t i = 0; i < movies.size(); i++) {
            if (movies[i].genre.find(lowerGenre) != string::npos) {
                results.push_back(i);
            }
        }

        return results;
    }

    // Mostrar resultado de búsqueda (máximo 5 resultados)
    void displayResults(const vector<int>& indices) {
        if (indices.empty()) {
            cout << "\nNo se encontraron películas.\n";
            return;
        }

        cout << "\n=== RESULTADOS (" << indices.size() << " películas encontradas) ===\n";

        size_t limit = min(indices.size(), size_t(5));
        for (size_t i = 0; i < limit; i++) {
            int idx = indices[i];
            cout << "\n[" << (i+1) << "] " << movies[idx].title << " ("
                 << movies[idx].year << ")\n";
            cout << "Director: " << movies[idx].director << "\n";
            cout << "Género: " << movies[idx].genre << "\n";

            // Mostrar primeros 120 caracteres de la sinopsis
            string plotPreview = movies[idx].plot;
            if (plotPreview.length() > 120) {
                plotPreview = plotPreview.substr(0, 120) + "...";
            }
            cout << "Sinopsis: " << plotPreview << "\n";
        }

        // Si hay más de 5 resultados, mostrar opción
        if (indices.size() > 5) {
            cout << "\n(Mostrando 5 de " << indices.size() << " resultados)\n";
        }
    }

    // Obtener una película por índice (para "Ver más tarde" o "Like")
    Movie getMovieByIndex(int idx) {
        if (idx >= 0 && idx < (int)movies.size()) {
            return movies[idx];
        }
        return Movie();
    }

    // Obtener total de películas
    int getTotalMovies() {
        return movies.size();
    }

private:
    // Borrar un nodo y todos sus hijos (recursivo)
    void deleteNode(TrieNode* node) {
        if (node == nullptr) return;
        for (auto& par : node->children) {
            deleteNode(par.second);
        }
        delete node;
    }

    // Convertir a minúsculas
    string toLower(const string& text) {
        string result = text;
        transform(result.begin(), result.end(), result.begin(),
                  [](unsigned char c) { return (char)tolower(c); });
        return result;
    }

    // Leer un registro completo del CSV.
    // Un campo entre comillas puede tener saltos de línea (por ejemplo, Cast),
    // así que se siguen leyendo líneas mientras haya comillas sin cerrar.
    bool readCSVRecord(istream& in, string& record) {
        string line;
        if (!getline(in, record)) return false;
        while (count(record.begin(), record.end(), '"') % 2 != 0 && getline(in, line)) {
            record += " " + line;  // El salto de línea se reemplaza por un espacio
        }
        if (!record.empty() && record.back() == '\r') record.pop_back();
        return true;
    }

    // Parsear línea CSV con manejo correcto de comillas
    Movie parseCSVLine(const string& line) {
        vector<string> fields;
        string field;
        bool inQuotes = false;

        for (size_t i = 0; i < line.size(); i++) {
            char c = line[i];
            if (c == '"') {
                // Dos comillas seguidas dentro de un campo = una comilla literal
                if (inQuotes && i + 1 < line.size() && line[i + 1] == '"') {
                    field += '"';
                    i++;
                } else {
                    inQuotes = !inQuotes;
                }
            } else if (c == ',' && !inQuotes) {
                fields.push_back(field);
                field = "";
            } else {
                field += c;
            }
        }
        fields.push_back(field);  // Último campo

        Movie movie;
        try {
            movie.year = fields.size() > 0 ? stoi(fields[0]) : 0;
        } catch (...) {
            movie.year = 0;
        }

        movie.title = fields.size() > 1 ? toLower(fields[1]) : "";
        movie.origin = fields.size() > 2 ? toLower(fields[2]) : "";
        movie.director = fields.size() > 3 ? toLower(fields[3]) : "";
        movie.cast = fields.size() > 4 ? toLower(fields[4]) : "";
        movie.genre = fields.size() > 5 ? toLower(fields[5]) : "";
        movie.wiki_url = fields.size() > 6 ? fields[6] : "";
        movie.plot = fields.size() > 7 ? toLower(fields[7]) : "";

        return movie;
    }
};

// ---------------------------------------
// FUNCIONES AUXILIARES DEL MENÚ
// ---------------------------------------

// Borrar la pantalla antes de mostrar el menú
void limpiarPantalla() {
#ifdef _WIN32
    int estado = system("cls");
#else
    int estado = system("clear");
#endif
    (void)estado;
}

// Leer un número entero; si el usuario escribe letras, se vuelve a pedir
// (con "cin >> numero" el programa entraba en un bucle infinito)
int leerEntero() {
    int valor;
    while (!(cin >> valor)) {
        if (cin.eof()) exit(0);  // Fin de la entrada: terminar el programa
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cout << "Entrada inválida. Ingrese un número: ";
    }
    cin.ignore(numeric_limits<streamsize>::max(), '\n');  // Limpiar buffer
    return valor;
}

// Esperar a que el usuario lea los resultados antes de volver al menú
void pausa() {
    cout << "\nPresione Enter para volver al menú principal...";
    string linea;
    if (!getline(cin, linea)) exit(0);
}

// Añadir una película a una lista solo si no está repetida
bool agregarSinRepetir(vector<int>& lista, int movieIdx) {
    if (find(lista.begin(), lista.end(), movieIdx) != lista.end()) {
        return false;
    }
    lista.push_back(movieIdx);
    return true;
}

// Preguntar si se quiere marcar una de las 5 películas mostradas.
// Se usa después de buscar por palabra, por director y por género.
void marcarPelicula(const vector<int>& results, vector<int>& watchLaterMovies, vector<int>& likedMovies) {
    if (results.empty()) return;
    size_t maxSelectable = min(results.size(), size_t(5));
    cout << "\nDesea marcar alguna película?\n";
    cout << "1. Ver más tarde\n";
    cout << "2. Like\n";
    cout << "0. No\n";
    cout << "Opción: ";
    int markChoice = leerEntero();

    if (markChoice == 1 || markChoice == 2) {
        cout << "Seleccione película (1-" << maxSelectable << "): ";
        int movieChoice = leerEntero();

        if (movieChoice >= 1 && movieChoice <= (int)maxSelectable) {
            int movieIdx = results[movieChoice - 1];
            if (markChoice == 1) {
                if (agregarSinRepetir(watchLaterMovies, movieIdx)) {
                    cout << "\n✓ Película añadida a 'Ver más tarde'\n";
                } else {
                    cout << "\nEsa película ya estaba en 'Ver más tarde'\n";
                }
            } else {
                if (agregarSinRepetir(likedMovies, movieIdx)) {
                    cout << "\n✓ Película marcada con 'Like'\n";
                } else {
                    cout << "\nEsa película ya tenía 'Like'\n";
                }
            }
        } else {
            cout << "\nOpción inválida. Seleccione entre 1 y "
                 << maxSelectable << "\n";
        }
    }
}

// ---------------------------------------
// MAIN - INTERFAZ 
// ---------------------------------------
int main() {
#ifdef _WIN32
    int estadoConsola = system("chcp 65001 > nul");  // Activar UTF-8 en la consola de Windows
    (void)estadoConsola;
#endif
    SuffixTrie trie;

    // CLion ejecuta el programa desde cmake-build-debug/, por eso se prueban varias rutas
    vector<string> rutas = {
        "movies_clean.csv",
        "../movies_clean.csv",
        "../Parte1/movies_clean.csv",
        "../../Parte1/movies_clean.csv"
    };

    cout << "Cargando películas...\n";
    bool cargado = false;
    for (const string& ruta : rutas) {
        if (trie.loadMovies(ruta)) {
            cargado = true;
            break;
        }
    }
    if (!cargado) {
        cerr << "Error: No se encontró movies_clean.csv.\n"
             << "Copie el archivo a la carpeta desde donde se ejecuta el programa.\n";
        return 1;
    }

    vector<int> likedMovies;      // Películas con "Like"
    vector<int> watchLaterMovies; // Películas con "Ver más tarde"

    while (true) {
        limpiarPantalla();
        cout << "========================================\n";
        cout << "      PLATAFORMA DE STREAMING\n";
        cout << "========================================\n";
        cout << "1. Buscar película (palabra/frase)\n";
        cout << "2. Buscar por director\n";
        cout << "3. Buscar por género\n";
        cout << "4. Ver mis películas guardadas (Ver más tarde)\n";
        cout << "5. Salir\n";
        cout << "----------------------------------------\n";
        cout << "Opción: ";
        int choice = leerEntero();

        if (choice == 1) {
            cout << "\nIngrese búsqueda (palabra o frase): ";
            string query;
            getline(cin, query);

            vector<int> results = trie.search(query);
            trie.displayResults(results);

            marcarPelicula(results, watchLaterMovies, likedMovies);
            pausa();

        } else if (choice == 2) {
            cout << "\nIngrese nombre del director: ";
            string director;
            getline(cin, director);

            vector<int> results = trie.searchByDirector(director);
            trie.displayResults(results);
            marcarPelicula(results, watchLaterMovies, likedMovies);
            pausa();

        } else if (choice == 3) {
            cout << "\nIngrese género: ";
            string genre;
            getline(cin, genre);

            vector<int> results = trie.searchByGenre(genre);
            trie.displayResults(results);
            marcarPelicula(results, watchLaterMovies, likedMovies);
            pausa();

        } else if (choice == 4) {
            if (watchLaterMovies.empty()) {
                cout << "\nNo tienes películas en 'Ver más tarde'.\n";
            } else {
                cout << "\n=== MIS PELÍCULAS (VER MÁS TARDE) ===\n";
                for (size_t i = 0; i < watchLaterMovies.size(); i++) {
                    Movie movie = trie.getMovieByIndex(watchLaterMovies[i]);
                    cout << "\n[" << (i+1) << "] " << movie.title << " ("
                         << movie.year << ")\n";
                    cout << "Director: " << movie.director << "\n";
                    cout << "Género: " << movie.genre << "\n";
                }
            }
            pausa();

        } else if (choice == 5) {
            cout << "\n¡Gracias por usar la plataforma de streaming!\n";
            break;

        } else {
            cout << "\nOpción inválida. Intente de nuevo.\n";
            pausa();
        }
    }

    return 0;
}
