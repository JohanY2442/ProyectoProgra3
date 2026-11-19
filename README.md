# Proyecto: Plataforma de Streaming

Programación III — UTEC (2026-2)

Plataforma de recomendación y búsqueda de películas en C++20. Carga una base de
datos de 34,565 películas, la indexa en un Trie de Sufijos y permite buscar por
título o por tag, dar Like, guardar en "Ver más tarde" y recibir
recomendaciones.

## Integrantes

- Huayta Enriquez, Johan Yadir
- Pillaca Pullo, Dervy
- Veliz Quispe, Jesús Adrián Ángel
- Lara Alvarez, Nicolas

## Cómo compilar y ejecutar

Requisitos: CMake 3.20+ y un compilador con soporte de C++20 (GCC, Clang o MinGW).

**Con CLion:** abrir la carpeta del proyecto (la que contiene `CMakeLists.txt`),
seleccionar el target `PlataformaStreaming` y presionar Run.

**Desde terminal:**

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/PlataformaStreaming
```

El programa busca `movies_clean.csv` en la carpeta actual y en la carpeta padre,
por lo que funciona tanto desde la raíz como desde `cmake-build-debug/`.
Los datos del usuario (likes y ver más tarde) se guardan en `user_data.txt`.

## Estructura del proyecto

```
ProyectoProgra3/
├── CMakeLists.txt
├── movies_clean.csv          Base de datos limpia
├── include/                  Archivos .h
│   ├── Movie.h               Movie y MovieBuilder
│   ├── SuffixTrie.h          Árbol + interfaz ISearchEngine
│   ├── SearchProxy.h         SearchCacheProxy
│   ├── MovieIterator.h       Iterator con paginación
│   ├── UserObserver.h        Subject y observadores
│   └── StreamingPlatform.h   Singleton que une todo
├── src/                      Archivos .cpp
│   ├── main.cpp
│   ├── SuffixTrie.cpp
│   ├── SearchProxy.cpp
│   ├── MovieIterator.cpp
│   ├── UserObserver.cpp
│   └── StreamingPlatform.cpp
└── Parte1/                   Entrega anterior (preprocesamiento)
    ├── clean_movies.cpp      Limpieza de datos en C++
    ├── main_progra3_oficial.cpp
    └── CODIGO_EXPLICADO_oficial.md
```

## Patrones de diseño (4+)

| Patrón | Clase | Dónde | Qué resuelve |
|---|---|---|---|
| **Singleton** | `StreamingPlatform` | `StreamingPlatform.h/.cpp` | Una única instancia de la plataforma con la base de datos cargada en memoria. |
| **Proxy** | `SearchCacheProxy` | `SearchProxy.h/.cpp` | Se interpone entre el usuario y el `SuffixTrie` (ambos implementan `ISearchEngine`) y guarda en caché las búsquedas repetidas. |
| **Iterator** | `MovieIterator<T>` | `MovieIterator.h/.cpp` | Recorre los resultados de una búsqueda en páginas de 5 películas (siguiente/anterior). |
| **Observer** | `UserProfileSubject`, `RecommendationObserver`, `PersistenceObserver` | `UserObserver.h/.cpp` | Al dar Like o guardar una película, se notifica a los observadores: uno actualiza las recomendaciones y otro guarda en disco. |
| **Builder** | `MovieBuilder` | `Movie.h` | Construye cada `Movie` campo por campo al leer una fila del CSV. |

## Árbol: Trie de Sufijos (`SuffixTrie`)

- Se insertan todos los sufijos de cada palabra del título, lo que permite
  encontrar **palabras, frases o sub-palabras** en cualquier posición.
- Búsqueda en O(k), donde k es el largo del texto buscado.
- El árbol está dividido en varios sub-árboles (shards) independientes, lo que
  habilita el procesamiento paralelo.
- También se indexan los tags (director, género, origen, etc.) para la búsqueda
  por tag.

## Programación paralela

| Operación | Técnica | Dónde |
|---|---|---|
| Inserción / indexación | `std::thread`, un hilo por grupo de películas | `SuffixTrie.cpp` |
| Búsqueda por título | `std::async` + `std::future`, un hilo por sub-árbol | `SuffixTrie.cpp` |
| Búsqueda por tag | `std::thread` + `std::promise` / `std::future` | `SuffixTrie.cpp` |
| Liberación de memoria | Un hilo por sub-árbol en el destructor | `SuffixTrie.cpp` |

El programa muestra en pantalla el tiempo de carga y de cada búsqueda.

## Interfaz

> **PENDIENTE:** actualizar esta sección cuando esté lista la interfaz gráfica
> (indicar librería usada: Qt / SFML / ImGui, y cómo se abre).

Actualmente la plataforma funciona por consola con un menú: buscar por título,
buscar por tag, ver recomendaciones, ver Likes y ver "Ver más tarde".

## Base de datos

- Original: 34,886 películas.
- Procesada: 34,565 películas (`movies_clean.csv`).
- Preprocesamiento: C++ (`Parte1/clean_movies.cpp`).
a sub-palabras en cualquier posición del título

## Base de Datos
- Original: 34,886 películas
- Procesada: 34,565 películas
- Preprocesamiento: R/RStudio 


## Archivos
- main_progra3_oficial.cpp: este es el programa principal
- ProcesamientoDatos_final.qmd: La limpieza de datos se hizo en R
- CODIGO_EXPLICADO.md: documentación
- movies_clean.csv: Esta es la base de datos limpia
[README (2).md](https://github.com/user-attachments/files/35063253/README.2.md)
