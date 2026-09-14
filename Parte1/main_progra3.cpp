#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>
#include <cctype>
#include <algorithm>

using namespace std;

// ========================================
// ESTRUCTURA DE PELÍCULA
// ========================================
struct Movie {
    int year;
    string title;
    string origin;
    string director;
    string cast;
    string genre;
    string wiki_url;
    string plot;
};

// ========================================
// NODO DEL TRIE DE SUFIJOS
// ========================================
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

    // Cargar películas desde CSV
    void loadMovies(const string& filename) {
        ifstream file(filename);
        if (!file.is_open()) {
            cerr << "Error: No se pudo abrir " << filename << endl;
            return;
        }

        string line;
        getline(file, line);  // Saltar encabezado

        int movieCount = 0;
        while (getline(file, line)) {  // Cargar TODAS las películas
            Movie movie = parseCSVLine(line);
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

            // Añadir el índice de la película si no está ya
            if (find(current->movieIndices.begin(), current->movieIndices.end(),
                     movieIndex) == current->movieIndices.end()) {
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
    // Convertir a minúsculas
    string toLower(const string& text) {
        string result = text;
        transform(result.begin(), result.end(), result.begin(), ::tolower);
        return result;
    }

    // Parsear línea CSV con manejo correcto de comillas
    Movie parseCSVLine(const string& line) {
        vector<string> fields;
        string field;
        bool inQuotes = false;

        for (char c : line) {
            if (c == '"') {
                inQuotes = !inQuotes;
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

// ========================================
// MAIN - INTERFAZ TERMINAL
// ========================================
int main() {
    system("chcp 65001 > nul");
    SuffixTrie trie;

    cout << "Cargando películas...\n";
    trie.loadMovies("movies_clean.csv");

    vector<int> likedMovies;      // Películas con "Like"
    vector<int> watchLaterMovies; // Películas con "Ver más tarde"

    int choice;
    while (true) {
        cout << "\n========================================\n";
        cout << "      PLATAFORMA DE STREAMING\n";
        cout << "========================================\n";
        cout << "1. Buscar película (palabra/frase)\n";
        cout << "2. Buscar por director\n";
        cout << "3. Buscar por género\n";
        cout << "4. Ver mis películas guardadas (Ver más tarde)\n";
        cout << "5. Salir\n";
        cout << "----------------------------------------\n";
        cout << "Opción: ";
        cin >> choice;
        cin.ignore();  // Limpiar buffer

        if (choice == 1) {
            cout << "\nIngrese búsqueda (palabra o frase): ";
            string query;
            getline(cin, query);

            vector<int> results = trie.search(query);
            trie.displayResults(results);

            if (!results.empty()) {
                size_t maxSelectable = min(results.size(), size_t(5));
                cout << "\nDesea marcar alguna película?\n";
                cout << "1. Ver más tarde (opción: 1-" << maxSelectable << ")\n";
                cout << "2. Like (opción: 1-" << maxSelectable << ")\n";
                cout << "0. No\n";
                cout << "Opción: ";
                int markChoice;
                cin >> markChoice;

                if (markChoice == 1 || markChoice == 2) {
                    cout << "Seleccione película (1-5): ";
                    int movieChoice;
                    cin >> movieChoice;

                    if (movieChoice >= 1 && movieChoice <= (int)min(results.size(), size_t(5))) {
                        int movieIdx = results[movieChoice - 1];
                        if (markChoice == 1) {
                            watchLaterMovies.push_back(movieIdx);
                            cout << "\n✓ Película añadida a 'Ver más tarde'\n";
                        } else {
                            likedMovies.push_back(movieIdx);
                            cout << "\n✓ Película marcada con 'Like'\n";
                        }
                    } else {
                        cout << "\nOpción inválida. Seleccione entre 1 y "
                        << min(results.size(), size_t(5)) << "\n";
                    }
                }
            }

        } else if (choice == 2) {
            cout << "\nIngrese nombre del director: ";
            string director;
            getline(cin, director);

            vector<int> results = trie.searchByDirector(director);
            trie.displayResults(results);

        } else if (choice == 3) {
            cout << "\nIngrese género: ";
            string genre;
            getline(cin, genre);

            vector<int> results = trie.searchByGenre(genre);
            trie.displayResults(results);

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

        } else if (choice == 5) {
            cout << "\n¡Gracias por usar la plataforma de streaming!\n";
            break;

        } else {
            cout << "\nOpción inválida. Intente de nuevo.\n";
        }
    }

    return 0;
}