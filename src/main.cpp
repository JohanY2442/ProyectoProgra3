
#include "StreamingPlatform.h"
#include <iostream>
#include <limits>
#include <cstdlib>

using namespace std;

void limpiarPantalla() {
#ifdef _WIN32
    int st = system("cls");
#else
    int st = system("clear");
#endif
    (void)st;
}

int leerEntero() {
    int valor;
    while (!(cin >> valor)) {
        if (cin.eof()) exit(0);
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cout << "Entrada invalida. Ingrese un numero: ";
    }
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    return valor;
}

void pausa() {
    cout << "\nPresione Enter para continuar...";
    string linea;
    if (!getline(cin, linea)) exit(0);
}

// Muestra la sinopsis completa y permite dar Like o Ver más tarde
void verDetallePelicula(int movieId) {
    StreamingPlatform* app = StreamingPlatform::getInstance();
    const Movie& m = app->getMovie(movieId);

    limpiarPantalla();
    cout << "========================================================\n";
    cout << " TITULO:   " << m.title << " (" << m.year << ")\n";
    cout << " DIRECTOR: " << m.director << "\n";
    cout << " CASTING:  " << m.cast << "\n";
    cout << " GENERO:   " << m.genre << " | ORIGEN: " << m.origin << "\n";
    cout << "========================================================\n";
    cout << "SINOPSIS:\n" << m.plot << "\n";
    cout << "--------------------------------------------------------\n";
    cout << "1. Like\n2. Ver mas tarde\n0. Volver\nOpcion: ";

    int op = leerEntero();
    if (op == 1) {
        if (app->likeMovie(movieId)) cout << "\n✓ Pelicula marcada con Like.\n";
        else cout << "\nYa tenia Like.\n";
        pausa();
    } else if (op == 2) {
        if (app->watchLaterMovie(movieId)) cout << "\n✓ Anadida a Ver mas tarde.\n";
        else cout << "\nYa estaba en Ver mas tarde.\n";
        pausa();
    }
}

// Recorre los resultados de 5 en 5 usando el Patrón Iterator
void explorarResultados(MovieIterator<int>& iterator) {
    StreamingPlatform* app = StreamingPlatform::getInstance();

    if (iterator.getTotalResults() == 0) {
        cout << "\nNo se encontraron peliculas.\n";
        pausa();
        return;
    }

    while (true) {
        limpiarPantalla();
        vector<int> page = iterator.getCurrentPage();

        cout << "=== RESULTADOS (Pagina " << iterator.getCurrentPageNumber()
             << " de " << iterator.getTotalPages()
             << " | Total: " << iterator.getTotalResults() << ") ===\n";

        for (size_t i = 0; i < page.size(); ++i) {
            const Movie& m = app->getMovie(page[i]);
            cout << "\n[" << (i + 1) << "] " << m.title << " (" << m.year << ")\n";
            cout << "    Director: " << m.director << " | Genero: " << m.genre << "\n";
            string preview = m.plot.length() > 110 ? m.plot.substr(0, 110) + "..." : m.plot;
            cout << "    Sinopsis: " << preview << "\n";
        }

        cout << "\n--------------------------------------------------------\n";
        cout << "[1-" << page.size() << "] Seleccionar pelicula\n";
        if (iterator.hasNextPage()) cout << "[6] Siguientes 5 coincidencias\n";
        if (iterator.hasPrevPage()) cout << "[7] Anteriores 5 coincidencias\n";
        cout << "[0] Volver al menu\nOpcion: ";

        int op = leerEntero();
        if (op == 0) break;
        else if (op >= 1 && op <= static_cast<int>(page.size())) verDetallePelicula(page[op - 1]);
        else if (op == 6 && iterator.hasNextPage()) iterator.nextPage();
        else if (op == 7 && iterator.hasPrevPage()) iterator.prevPage();
    }
}

int main() {
#ifdef _WIN32
    int st = system("chcp 65001 > nul");
    (void)st;
#endif

    StreamingPlatform* app = StreamingPlatform::getInstance();

    vector<string> rutas = {
        "movies_clean.csv",
        "../movies_clean.csv",
        "Parte1/movies_clean.csv",
        "../Parte1/movies_clean.csv",
        "../../Parte1/movies_clean.csv"
    };

    cout << "Cargando e indexando base de datos en paralelo...\n";
    if (!app->initialize(rutas)) {
        cerr << "Error: No se encontro movies_clean.csv.\n";
        return 1;
    }
    pausa();

    while (true) {
        limpiarPantalla();
        cout << "========================================================\n";
        cout << "                PLATAFORMA DE STREAMING\n";
        cout << "========================================================\n";

        // Muestra películas en Ver más tarde al iniciar
        const vector<int>& watchLater = app->getWatchLaterList();
        cout << "\nVER MAS TARDE (" << watchLater.size() << "):\n";
        if (watchLater.empty()) {
            cout << "  (Lista vacia)\n";
        } else {
            for (size_t i = 0; i < min(watchLater.size(), size_t(5)); ++i) {
                const Movie& m = app->getMovie(watchLater[i]);
                cout << "  - " << m.title << " (" << m.year << ")\n";
            }
        }

        // Muestra recomendaciones basadas en Likes al iniciar
        const vector<int>& recommended = app->getRecommendedList();
        cout << "\nPELICULAS SIMILARES A TUS LIKES:\n";
        if (recommended.empty()) {
            cout << "  (Dale Like a una pelicula para ver recomendaciones)\n";
        } else {
            for (int id : recommended) {
                const Movie& m = app->getMovie(id);
                cout << "  * " << m.title << " (" << m.year << ") | " << m.genre << "\n";
            }
        }

        cout << "\n--------------------------------------------------------\n";
        cout << "1. Buscar pelicula (palabra, frase o sub-palabra)\n";
        cout << "2. Buscar por Tag (director, genero, casting, origen)\n";
        cout << "3. Ver lista completa de 'Ver mas tarde'\n";
        cout << "4. Salir\n";
        cout << "--------------------------------------------------------\n";
        cout << "Opcion: ";

        int choice = leerEntero();
        if (choice == 1) {
            cout << "\nIngrese busqueda: ";
            string query;
            getline(cin, query);
            MovieIterator<int> it = app->searchMovies(query);
            explorarResultados(it);

        } else if (choice == 2) {
            cout << "\n1. Director\n2. Genero\n3. Casting\n4. Origen\nOpcion: ";
            int tagOp = leerEntero();
            string tagType = "director";
            if (tagOp == 2) tagType = "genre";
            else if (tagOp == 3) tagType = "cast";
            else if (tagOp == 4) tagType = "origin";

            cout << "Ingrese valor a buscar: ";
            string val;
            getline(cin, val);
            MovieIterator<int> it = app->searchByTag(tagType, val);
            explorarResultados(it);

        } else if (choice == 3) {
            MovieIterator<int> it(app->getWatchLaterList(), 5);
            explorarResultados(it);

        } else if (choice == 4) {
            break;
        }
    }
    return 0;
}
