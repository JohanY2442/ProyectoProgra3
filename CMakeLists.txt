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
