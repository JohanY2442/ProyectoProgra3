#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
using namespace std;

// Estructura para almacenar una película
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

// Función para leer el CSV
vector<Movie> loadMovies(const string& filename) {
    vector<Movie> movies;
    ifstream file(filename);
    
    if (!file.is_open()) {
        cerr << "Error: No se pudo abrir " << filename << endl;
        return movies;
    }
    
    string line;
    getline(file, line); // Saltar encabezado
    
    while (getline(file, line)) {
        // TODO: Parsear línea CSV
        // Por ahora, solo placeholder
    }
    
    file.close();
    return movies;
}

int main() {
    // Cargar películas
    vector<Movie> movies = loadMovies("movies_clean.csv");
    cout << "Películas cargadas: " << movies.size() << endl;
    
    // TODO: Insertar en árbol
    // TODO: Mostrar menú
    
    return 0;
}
