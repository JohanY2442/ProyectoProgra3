# Proyecto: Plataforma de Streaming (Chestream)

Programación III — UTEC (2026-2)

Plataforma de búsqueda y recomendación de películas en C++20. Carga una base de
datos de 34,565 películas, la indexa en un Trie de Sufijos y permite buscar por
título, sinopsis o tag, dar Like, guardar en "Ver más tarde" y recibir
recomendaciones. Cuenta con una interfaz gráfica web y una versión de consola.

## Integrantes

- Huayta Enriquez, Johan Yadir
- Pillaca Pullo, Dervy
- Veliz Quispe, Jesús Adrián Ángel
- Lara Alvarez, Nicolas

## Cómo compilar y ejecutar

Requisitos: CMake 3.20+ y un compilador con soporte de C++20 (GCC, Clang o MinGW).
No se necesita instalar ninguna otra librería: el servidor HTTP
(`cpp-httplib`) viene incluido en `include/third_party/`.

El proyecto define dos ejecutables:

| Target | Qué es |
|---|---|
| `PlataformaStreamingWeb` | **Interfaz gráfica** (se abre en el navegador) |
| `PlataformaStreaming` | Versión de consola (menú de texto) |

**Con CLion:** abrir la carpeta del proyecto (la que contiene `CMakeLists.txt`),
seleccionar el target `PlataformaStreamingWeb` y presionar Run.

**Desde terminal:**

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/PlataformaStreamingWeb
```

Al iniciar, el programa abre el navegador en `http://localhost:8080`
(si el puerto está ocupado, usa el siguiente hasta el 8090). Si el navegador no
se abre solo, entrar manualmente a esa dirección.

El programa busca `movies_clean.csv` en la carpeta actual y en las carpetas
padre, por lo que funciona tanto desde la raíz como desde `cmake-build-debug/`.
Los likes y la lista de "Ver más tarde" se guardan en `user_data.txt`.

## Interfaz

La interfaz gráfica es una página web local llamada **Chestream**. Toda la lógica
(lectura, Trie de sufijos, ranking, recomendaciones, patrones e hilos) sigue en C++;
`src/server_main.cpp` levanta un servidor HTTP local con **cpp-httplib** que responde
en JSON, y la página (`web/`: HTML, CSS y JavaScript) solo muestra los resultados.

**Cómo abrirla**
- **CLion:** recargar CMake, elegir el target `PlataformaStreamingWeb` y Run.
- **VS Code / terminal:**
```
  g++ -std=c++20 -O2 -pthread -Iinclude -D_WIN32_WINNT=0x0A00 src/server_main.cpp src/SuffixTrie.cpp src/SearchProxy.cpp src/MovieIterator.cpp src/UserObserver.cpp src/StreamingPlatform.cpp -o PlataformaStreamingWeb.exe -lws2_32
  .\PlataformaStreamingWeb.exe
```
El navegador se abre solo en `http://localhost:8080`.

**Funciones:** búsqueda por palabra/frase/sub-palabra y por tag, resultados de 5 en 5
ordenados por importancia, detalle con sinopsis completa, Like y Ver más tarde
(se pueden quitar), y al inicio se muestran "Ver más tarde" y "Similares a tus Likes".
Desde la barra superior se abren las listas completas de **Tus Likes** y
**Ver más tarde**. Como el catálogo no tiene imágenes, cada película recibe un
**póster generado** (inicial del título sobre un degradado de colores de la marca,
siempre el mismo para la misma película), creado en `web/posters.js`.

La versión de consola (`src/main.cpp`, target `PlataformaStreaming`) sigue disponible.

**API del servidor**

| Ruta de la API | Función |
|---|---|
| `GET /api/estado` | Estado de la carga del árbol |
| `GET /api/inicio` | Ver más tarde, recomendadas y likes |
| `GET /api/buscar?q=&pagina=` | Búsqueda por texto |
| `GET /api/tag?tipo=&valor=&pagina=` | Búsqueda por tag |
| `GET /api/lista?tipo=likes\|vermastarde&todos=1` | Lista de Likes o Ver más tarde (el Like más reciente primero) |
| `GET /api/pelicula?id=` | Detalle de una película |
| `POST /api/like`, `/api/quitar-like` | Marcar o quitar Like |
| `POST /api/vermastarde`, `/api/quitar-vermastarde` | Marcar o quitar Ver más tarde |

## Estructura del proyecto

```
ProyectoProgra3/
├── CMakeLists.txt
├── movies_clean.csv            Base de datos limpia
├── include/                    Archivos .h
│   ├── Movie.h                 Movie y MovieBuilder
│   ├── SuffixTrie.h            Árbol + interfaz ISearchEngine
│   ├── SearchProxy.h           SearchCacheProxy
│   ├── MovieIterator.h         Iterator con paginación
│   ├── UserObserver.h          Subject y observadores
│   ├── StreamingPlatform.h     Singleton que une todo
│   └── third_party/httplib.h   Servidor HTTP (cpp-httplib)
├── src/                        Archivos .cpp
│   ├── server_main.cpp         main de la interfaz gráfica (servidor)
│   ├── main.cpp                main de la versión de consola
│   ├── SuffixTrie.cpp
│   ├── SearchProxy.cpp
│   ├── MovieIterator.cpp
│   ├── UserObserver.cpp
│   └── StreamingPlatform.cpp
├── web/                        Interfaz gráfica (HTML, CSS, JS)
│   ├── index.html
│   ├── style.css
│   ├── app.js
│   ├── posters.js              Pósters generados para cada película
│   ├── logo.svg
│   ├── img/
│   └── fonts/
└── Parte1/                     Entrega anterior (preprocesamiento)
    ├── clean_movies.cpp        Limpieza de datos en C++
    ├── main_progra3_oficial.cpp
    └── CODIGO_EXPLICADO_oficial.md
```

## Patrones de diseño (5)

| Patrón | Clase | Dónde | Qué resuelve |
|---|---|---|---|
| **Singleton** | `StreamingPlatform` | `StreamingPlatform.h/.cpp` | Una única instancia de la plataforma con la base de datos cargada en memoria. |
| **Proxy** | `SearchCacheProxy` | `SearchProxy.h/.cpp` | Se interpone entre el usuario y el `SuffixTrie` (ambos implementan `ISearchEngine`) y guarda en caché las búsquedas repetidas. |
| **Iterator** | `MovieIterator<T>` | `MovieIterator.h/.cpp` | Recorre en páginas de 5 películas los resultados de una búsqueda (siguiente/anterior) y las listas de Likes y Ver más tarde. |
| **Observer** | `UserProfileSubject`, `RecommendationObserver`, `PersistenceObserver` | `UserObserver.h/.cpp` | Al dar Like o guardar una película, se notifica a los observadores: uno actualiza las recomendaciones y otro guarda en disco. |
| **Builder** | `MovieBuilder` | `Movie.h` | Construye cada `Movie` campo por campo al leer una fila del CSV. |

## Árbol: Trie de Sufijos (`SuffixTrie`)

- Se insertan todos los sufijos de cada palabra del **título** y de la
  **sinopsis**, lo que permite encontrar palabras, frases o sub-palabras en
  cualquier posición.
- Los resultados se ordenan por relevancia: una coincidencia en el título pesa
  más que una en la sinopsis.
- Búsqueda en O(k), donde k es el largo del texto buscado.
- El árbol está dividido en varios sub-árboles (shards) independientes, lo que
  habilita el procesamiento paralelo.
- También se indexan los tags (director, género, casting, origen) para la
  búsqueda por tag.

## Programación paralela

| Operación | Técnica | Dónde |
|---|---|---|
| Inserción / indexación | `std::thread`, un hilo por sub-árbol | `SuffixTrie.cpp` |
| Búsqueda por texto | `std::async` + `std::future`, un hilo por sub-árbol | `SuffixTrie.cpp` |
| Búsqueda por tag | `std::thread` + `std::promise` / `std::future` | `SuffixTrie.cpp` |
| Liberación de memoria | Un hilo por sub-árbol en el destructor | `SuffixTrie.cpp` |
| Carga en la interfaz | Hilo aparte para construir el árbol sin bloquear el servidor | `server_main.cpp` |

Un `std::mutex` protege a `StreamingPlatform` cuando varias peticiones HTTP
llegan al mismo tiempo. La página muestra el tiempo de cada búsqueda.

## Base de datos

- Original: 34,886 películas.
- Procesada: 34,565 películas (`movies_clean.csv`).
- Preprocesamiento: C++ (`Parte1/clean_movies.cpp`).

## Créditos y licencias de terceros

- **cpp-httplib** (Yuji Hirose) — licencia MIT. `include/third_party/httplib.h`
- **Playfair Display** (Google Fonts) — SIL Open Font License 1.1. `web/fonts/OFL-PlayfairDisplay.txt`
- Logo y mascota "Ches" — diseño propio del equipo.
