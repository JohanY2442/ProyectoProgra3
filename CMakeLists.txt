cmake_minimum_required(VERSION 3.20)
project(PlataformaStreaming)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -O2")

include_directories(include)

add_executable(PlataformaStreaming
    src/main.cpp
    src/SuffixTrie.cpp
    src/SearchProxy.cpp
    src/MovieIterator.cpp
    src/UserObserver.cpp
    src/StreamingPlatform.cpp
)

# ---------------------------------------------------------------
# Interfaz gráfica web (Parte 2)
# Servidor C++ con cpp-httplib (include/third_party/httplib.h)
# que sirve la página de la carpeta web/ en http://localhost:8080
# ---------------------------------------------------------------
add_executable(PlataformaStreamingWeb
    src/server_main.cpp
    src/SuffixTrie.cpp
    src/SearchProxy.cpp
    src/MovieIterator.cpp
    src/UserObserver.cpp
    src/StreamingPlatform.cpp
)

if(WIN32)
    # httplib necesita Windows 10+ y la librería de sockets
    target_compile_definitions(PlataformaStreamingWeb PRIVATE _WIN32_WINNT=0x0A00)
    target_link_libraries(PlataformaStreamingWeb PRIVATE ws2_32)
endif()

# Ruta absoluta a web/: así funciona igual desde CLion (cmake-build-debug/) y VS Code (build/)
# y los cambios en HTML/CSS/JS se ven al recargar el navegador, sin recompilar
target_compile_definitions(PlataformaStreamingWeb PRIVATE WEB_DIR="${CMAKE_SOURCE_DIR}/web")
