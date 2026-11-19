// Interfaz gráfica web de la Plataforma de Streaming.
// Toda la lógica (Trie de sufijos, patrones, hilos) sigue en C++.
// Este archivo solo levanta un servidor HTTP local que responde en JSON;
// la página en web/ (HTML, CSS y JS) dibuja los resultados en el navegador.

// httplib (que incluye windows.h) va PRIMERO: si se incluye después de
// "using namespace std" (Movie.h), en Windows "byte" choca con std::byte
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#endif
#include "third_party/httplib.h"
#include "StreamingPlatform.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using namespace std;

// ---------------------------------------
// ESTADO DEL SERVIDOR
// ---------------------------------------
static atomic<bool> g_listo{false};     // true cuando el árbol terminó de construirse
static atomic<bool> g_error{false};     // true si no se encontró el CSV
static atomic<double> g_segundosCarga{0.0};
static mutex g_mtx;                     // protege a StreamingPlatform entre peticiones

// ---------------------------------------
// FUNCIONES AUXILIARES PARA JSON
// ---------------------------------------

// Escapa un texto para poder ponerlo dentro de un string JSON
string jsonEscape(const string& s) {
    string out;
    out.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    return out;
}

bool contiene(const vector<int>& lista, int id) {
    for (int x : lista) if (x == id) return true;
    return false;
}

// Respuesta de Like / Ver más tarde: devuelve cómo quedó la película,
// así la página no necesita pedir el detalle otra vez
string estadoMarcasJson(StreamingPlatform* app, int id, bool nuevo) {
    return string("{\"ok\":true,\"nuevo\":") + (nuevo ? "true" : "false")
         + ",\"like\":" + (contiene(app->getLikedList(), id) ? "true" : "false")
         + ",\"verMasTarde\":" + (contiene(app->getWatchLaterList(), id) ? "true" : "false") + "}";
}

// Tarjeta resumida de una película (para listas y resultados)
string tarjetaJson(StreamingPlatform* app, int id) {
    const Movie& m = app->getMovie(id);
    string corta = m.plot.size() > 170 ? m.plot.substr(0, 170) + "..." : m.plot;
    ostringstream o;
    o << "{\"id\":" << id
      << ",\"titulo\":\"" << jsonEscape(m.title) << "\""
      << ",\"anio\":" << m.year
      << ",\"director\":\"" << jsonEscape(m.director) << "\""
      << ",\"genero\":\"" << jsonEscape(m.genre) << "\""
      << ",\"origen\":\"" << jsonEscape(m.origin) << "\""
      << ",\"sinopsisCorta\":\"" << jsonEscape(corta) << "\"}";
    return o.str();
}

string listaJson(StreamingPlatform* app, const vector<int>& ids) {
    string out = "[";
    for (size_t i = 0; i < ids.size(); ++i) {
        if (i) out += ",";
        out += tarjetaJson(app, ids[i]);
    }
    return out + "]";
}

// Página de resultados usando el Patrón Iterator (5 en 5)
string paginaJson(StreamingPlatform* app, MovieIterator<int>& it, int pagina, double ms) {
    for (int p = 1; p < pagina && it.hasNextPage(); ++p) it.nextPage();
    ostringstream o;
    o << "{\"total\":" << it.getTotalResults()
      << ",\"pagina\":" << it.getCurrentPageNumber()
      << ",\"totalPaginas\":" << it.getTotalPages()
      << ",\"haySiguiente\":" << (it.hasNextPage() ? "true" : "false")
      << ",\"hayAnterior\":" << (it.hasPrevPage() ? "true" : "false")
      << ",\"ms\":" << ms
      << ",\"resultados\":" << listaJson(app, it.getCurrentPage()) << "}";
    return o.str();
}

void enviarJson(httplib::Response& res, const string& body, int status = 200) {
    res.status = status;
    res.set_content(body, "application/json; charset=utf-8");
}

// Devuelve false (y responde 503) si el árbol aún se está construyendo
bool verificarListo(httplib::Response& res) {
    if (g_listo) return true;
    enviarJson(res, "{\"error\":\"cargando\"}", 503);
    return false;
}

int leerId(const httplib::Request& req) {
    if (!req.has_param("id")) return -1;
    try { return stoi(req.get_param_value("id")); } catch (...) { return -1; }
}

int leerPagina(const httplib::Request& req) {
    if (!req.has_param("pagina")) return 1;
    try { return max(1, stoi(req.get_param_value("pagina"))); } catch (...) { return 1; }
}

// Busca la carpeta web/ (primero la ruta de CMake, luego rutas relativas)
string buscarCarpetaWeb() {
    vector<string> rutas = {"web", "../web", "../../web"};
#ifdef WEB_DIR
    rutas.insert(rutas.begin(), WEB_DIR);  // ruta definida en CMakeLists.txt
#endif
    for (const string& r : rutas) {
        ifstream f(r + "/index.html");
        if (f.good()) return r;
    }
    return "";
}

// ---------------------------------------
// MAIN
// ---------------------------------------
int main() {
#ifdef _WIN32
    int st = system("chcp 65001 > nul");
    (void)st;
#endif
    StreamingPlatform* app = StreamingPlatform::getInstance();

    string carpetaWeb = buscarCarpetaWeb();
    if (carpetaWeb.empty()) {
        cerr << "Error: no se encontro la carpeta web/ (con index.html).\n";
        return 1;
    }

    // Construir el árbol en un hilo aparte: la página ya abre y muestra "cargando"
    thread cargador([app]() {
        vector<string> rutas = {
            "movies_clean.csv", "../movies_clean.csv", "../../movies_clean.csv",
            "Parte1/movies_clean.csv", "../Parte1/movies_clean.csv"
        };
        auto t0 = chrono::steady_clock::now();
        bool ok;
        {
            lock_guard<mutex> lock(g_mtx);
            ok = app->initialize(rutas);
        }
        g_segundosCarga = chrono::duration<double>(chrono::steady_clock::now() - t0).count();
        if (ok) g_listo = true;
        else {
            g_error = true;
            cerr << "Error: No se encontro movies_clean.csv.\n";
        }
    });
    cargador.detach();

    httplib::Server svr;
    svr.set_mount_point("/", carpetaWeb);

    // Estado de la carga (la página lo consulta hasta que esté listo)
    svr.Get("/api/estado", [&](const httplib::Request&, httplib::Response& res) {
        ostringstream o;
        o << "{\"listo\":" << (g_listo ? "true" : "false")
          << ",\"error\":" << (g_error ? "true" : "false")
          << ",\"segundos\":" << g_segundosCarga.load()
          << ",\"total\":" << (g_listo ? app->getTotalMovies() : 0) << "}";
        enviarJson(res, o.str());
    });

    // Pantalla de inicio: Ver más tarde + recomendaciones por Likes
    svr.Get("/api/inicio", [&](const httplib::Request&, httplib::Response& res) {
        if (!verificarListo(res)) return;
        lock_guard<mutex> lock(g_mtx);
        string body = "{\"verMasTarde\":" + listaJson(app, app->getWatchLaterList())
                    + ",\"recomendadas\":" + listaJson(app, app->getRecommendedList())
                    + ",\"likes\":" + listaJson(app, app->getLikedList()) + "}";
        enviarJson(res, body);
    });

    // Búsqueda por palabra, frase o sub-palabra
    svr.Get("/api/buscar", [&](const httplib::Request& req, httplib::Response& res) {
        if (!verificarListo(res)) return;
        string q = req.get_param_value("q");
        lock_guard<mutex> lock(g_mtx);
        auto t0 = chrono::steady_clock::now();
        MovieIterator<int> it = app->searchMovies(q);
        double ms = chrono::duration<double, milli>(chrono::steady_clock::now() - t0).count();
        enviarJson(res, paginaJson(app, it, leerPagina(req), ms));
    });

    // Búsqueda por tag: director, genre, cast, origin
    svr.Get("/api/tag", [&](const httplib::Request& req, httplib::Response& res) {
        if (!verificarListo(res)) return;
        string tipo = req.get_param_value("tipo");
        string valor = req.get_param_value("valor");
        if (tipo != "director" && tipo != "genre" && tipo != "cast" && tipo != "origin") {
            enviarJson(res, "{\"error\":\"tag invalido\"}", 400);
            return;
        }
        lock_guard<mutex> lock(g_mtx);
        auto t0 = chrono::steady_clock::now();
        MovieIterator<int> it = app->searchByTag(tipo, valor);
        double ms = chrono::duration<double, milli>(chrono::steady_clock::now() - t0).count();
        enviarJson(res, paginaJson(app, it, leerPagina(req), ms));
    });

    // Detalle completo de una película
    svr.Get("/api/pelicula", [&](const httplib::Request& req, httplib::Response& res) {
        if (!verificarListo(res)) return;
        int id = leerId(req);
        lock_guard<mutex> lock(g_mtx);
        if (id < 0 || id >= app->getTotalMovies()) {
            enviarJson(res, "{\"error\":\"id invalido\"}", 404);
            return;
        }
        const Movie& m = app->getMovie(id);
        ostringstream o;
        o << "{\"id\":" << id
          << ",\"titulo\":\"" << jsonEscape(m.title) << "\""
          << ",\"anio\":" << m.year
          << ",\"director\":\"" << jsonEscape(m.director) << "\""
          << ",\"casting\":\"" << jsonEscape(m.cast) << "\""
          << ",\"genero\":\"" << jsonEscape(m.genre) << "\""
          << ",\"origen\":\"" << jsonEscape(m.origin) << "\""
          << ",\"wiki\":\"" << jsonEscape(m.wiki_url) << "\""
          << ",\"sinopsis\":\"" << jsonEscape(m.plot) << "\""
          << ",\"like\":" << (contiene(app->getLikedList(), id) ? "true" : "false")
          << ",\"verMasTarde\":" << (contiene(app->getWatchLaterList(), id) ? "true" : "false")
          << "}";
        enviarJson(res, o.str());
    });

    // Like y Ver más tarde (el Observer guarda en disco y recalcula recomendaciones)
    svr.Post("/api/like", [&](const httplib::Request& req, httplib::Response& res) {
        if (!verificarListo(res)) return;
        int id = leerId(req);
        lock_guard<mutex> lock(g_mtx);
        if (id < 0 || id >= app->getTotalMovies()) { enviarJson(res, "{\"ok\":false}", 404); return; }
        bool nuevo = app->likeMovie(id);
        enviarJson(res, estadoMarcasJson(app, id, nuevo));
    });

    svr.Post("/api/vermastarde", [&](const httplib::Request& req, httplib::Response& res) {
        if (!verificarListo(res)) return;
        int id = leerId(req);
        lock_guard<mutex> lock(g_mtx);
        if (id < 0 || id >= app->getTotalMovies()) { enviarJson(res, "{\"ok\":false}", 404); return; }
        bool nuevo = app->watchLaterMovie(id);
        enviarJson(res, estadoMarcasJson(app, id, nuevo));
    });

    // Quitar Like / quitar de Ver más tarde (también avisan al Observer)
    svr.Post("/api/quitar-like", [&](const httplib::Request& req, httplib::Response& res) {
        if (!verificarListo(res)) return;
        int id = leerId(req);
        lock_guard<mutex> lock(g_mtx);
        if (id < 0 || id >= app->getTotalMovies()) { enviarJson(res, "{\"ok\":false}", 404); return; }
        bool cambio = app->unlikeMovie(id);
        enviarJson(res, estadoMarcasJson(app, id, cambio));
    });

    svr.Post("/api/quitar-vermastarde", [&](const httplib::Request& req, httplib::Response& res) {
        if (!verificarListo(res)) return;
        int id = leerId(req);
        lock_guard<mutex> lock(g_mtx);
        if (id < 0 || id >= app->getTotalMovies()) { enviarJson(res, "{\"ok\":false}", 404); return; }
        bool cambio = app->removeWatchLater(id);
        enviarJson(res, estadoMarcasJson(app, id, cambio));
    });

    // Probar puertos hasta encontrar uno libre
    int puerto = -1;
    for (int p = 8080; p <= 8090; ++p) {
        if (svr.bind_to_port("127.0.0.1", p)) { puerto = p; break; }
    }
    if (puerto < 0) {
        cerr << "Error: no hay puertos libres entre 8080 y 8090.\n";
        return 1;
    }

    string url = "http://localhost:" + to_string(puerto);
    cout << "========================================================\n";
    cout << "  CHESTREAM - Plataforma de Streaming (interfaz web)\n";
    cout << "  Abre en tu navegador: " << url << "\n";
    cout << "  (Cierra esta ventana o presiona Ctrl+C para salir)\n";
    cout << "========================================================\n";

#ifdef _WIN32
    string cmd = "start \"\" \"" + url + "\"";
    int st2 = system(cmd.c_str());  // Abrir el navegador automáticamente
    (void)st2;
#endif

    svr.listen_after_bind();
    return 0;
}
