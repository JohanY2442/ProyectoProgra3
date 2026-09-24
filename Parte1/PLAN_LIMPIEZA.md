# Plan de limpieza de datos

*Documento de referencia personal (Adrian). Se escribió ANTES de implementar la limpieza.*

## 1. Punto de partida

- **Archivo original:** `wiki_movie_plots_deduped.csv`
- **Filas originales:** 34,886 películas
- **Columnas (8):** Release Year, Title, Origin/Ethnicity, Director, Cast, Genre, Wiki Page, Plot
- **Uso final:** el CSV limpio será leído por el programa en C++ (Trie de sufijos), así que debe ser simple de parsear: sin saltos de línea dentro de campos, sin acentos y en minúsculas.

## 2. ¿Qué columnas necesitamos?

| Columna          | ¿Se usa?     | Para qué                                    |
|------------------|--------------|---------------------------------------------|
| Release Year     | Sí           | Mostrar el año y detectar duplicados reales |
| Title            | Sí (crítica) | Es lo que se indexa y busca en el Trie      |
| Origin/Ethnicity | Sí           | Se conserva en el struct `Movie`            |
| Director         | Sí           | Búsqueda por director                       |
| Cast             | Sí           | Se conserva en el struct `Movie`            |
| Genre            | Sí           | Búsqueda por género                         |
| Wiki Page        | Sí           | Se conserva (`wiki_url`)                    |
| Plot             | Sí (crítica) | Sinopsis que se muestra en los resultados   |

**Decisión:** no se elimina ninguna columna. El CSV final mantiene las 8 columnas.

## 3. ¿Qué transformaciones hacer?

1.  **Trabajar sobre una copia** (`df_limpia`), para no dañar los datos originales si algo sale mal.
2.  **Quitar espacios extra** al inicio y al final del texto.
3.  **Pasar a minúsculas** Title, Plot, Director y Genre, para que la búsqueda no distinga mayúsculas.
4.  **Quitar acentos y caracteres raros** de Title y Plot (transliteración a ASCII).
5.  **Quitar saltos de línea** dentro de Plot y Director antes de exportar, porque romperían las filas del CSV al leerlo en C++.
6.  **Exportar** a `movies_clean.csv` con los valores vacíos escritos como texto vacío.

## 4. ¿Qué duplicados eliminar?

- **Duplicados exactos:** filas idénticas en todas las columnas.
- **Duplicados reales:** filas con el mismo **Title** y el mismo **Release Year**, aunque difieran en otras columnas. Se conserva la primera aparición.
- No se eliminan películas con el mismo título pero distinto año (pueden ser remakes legítimos).

## 5. ¿Qué valores nulos manejar?

- **Columnas críticas: Title y Plot.** Si están vacías o son `NA`, la fila se elimina, porque sin título no se puede buscar y sin sinopsis no hay nada que mostrar.
- **Columnas no críticas** (Director, Cast, Genre, Origin, Wiki Page): se permiten vacías y se exportan como texto vacío. En el original, Director/Genre pueden venir como `unknown`.

## 6. Orden de las operaciones

1.  Cargar datos originales
2.  Eliminar duplicados exactos
3.  Filtrar nulos en Title y Plot
4.  Limpiar espacios y pasar a minúsculas
5.  Quitar acentos
6.  Eliminar duplicados por Title + Release Year
7.  Quitar saltos de línea y guardar el CSV

## 7. Resultado esperado

|                   | Cantidad |
|-------------------|----------|
| Películas antes   | 34,886   |
| Películas después | 34,565   |
| Eliminadas        | 321      |
