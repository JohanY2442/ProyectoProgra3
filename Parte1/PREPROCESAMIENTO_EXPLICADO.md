# Preprocesamiento explicado

Este documento explica, paso a paso y sin mostrar código, cómo se limpió el conjunto de datos de películas en `ProcesamientoDatos.qmd` para generar `movies_clean.csv`.

## ¿Por qué hay que limpiar los datos?

El archivo original (`wiki_movie_plots_deduped.csv`) viene de Wikipedia y tiene inconsistencias: títulos en mayúsculas y minúsculas mezcladas, letras con acentos, espacios sobrantes, filas repetidas y filas sin título o sin sinopsis. Nuestro programa en C++ construye un **Trie de sufijos** con los títulos y busca texto dentro de ellos. Para que la búsqueda funcione de forma predecible, todos los títulos deben tener el mismo formato: minúsculas, sin acentos y sin espacios extra. Por ejemplo, "Bárco" y "barco" deben poder encontrarse con la misma búsqueda.

## Los datos originales

- **Filas:** 34,886 películas
- **Columnas (8):** año de estreno, título, origen, director, reparto, género, página de Wikipedia y sinopsis

## Paso a paso

### 1. Cargar los datos

Se lee el CSV original y se guarda en una variable llamada `df_sucia`. Se revisan sus dimensiones, los nombres de columnas y la cantidad de valores nulos por columna. Estas cifras sirven de referencia para comparar después de la limpieza.

### 2. Crear una copia de trabajo

Todos los cambios se hacen sobre una copia (`df_limpia`). Los datos originales nunca se modifican, así que si algo sale mal se puede volver a empezar.

### 3. Eliminar filas idénticas

Se borran las filas que son exactamente iguales en todas sus columnas. Son registros repetidos que no aportan información nueva.

### 4. Eliminar filas sin título o sin sinopsis

Se descartan las películas cuyo título o sinopsis esté vacío o sea nulo. El título es lo que el usuario busca en el programa y la sinopsis es lo que se muestra en los resultados. Una película sin alguno de los dos no sirve para nuestro proyecto.

### 5. Quitar espacios y pasar a minúsculas

En título, sinopsis, director y género se eliminan los espacios sobrantes al inicio y al final, y todo el texto pasa a minúsculas. Así la búsqueda no depende de cómo estaba escrito el texto en Wikipedia.

### 6. Quitar acentos y caracteres raros

En título y sinopsis, las letras con acento o símbolos especiales se convierten a su equivalente sin acento (por ejemplo, "é" pasa a "e"). Esto evita problemas de codificación al leer el archivo en C++ y permite que quien busca "cafe" encuentre "café".

### 7. Eliminar duplicados reales

Se eliminan las películas que tienen el **mismo título y el mismo año de estreno**, dejando solo la primera aparición. Se hace después de normalizar el texto, porque así se detectan casos que antes parecían distintos solo por mayúsculas, acentos o espacios. Las películas con el mismo título pero distinto año se conservan, porque suelen ser versiones nuevas de una misma historia.

### 8. Preparar y guardar el CSV

Antes de exportar, se reemplazan los saltos de línea dentro de la sinopsis y del director por espacios. Un salto de línea dentro de un campo partiría una fila en dos y confundiría al lector de CSV de C++. Finalmente se guarda `movies_clean.csv`, con los valores vacíos escritos como texto vacío.

## ¿Por qué eliminamos 321 películas?

De 34,886 películas quedaron 34,565, es decir, se eliminaron **321**. Esas filas salieron por tres razones posibles:

1. **Filas totalmente repetidas:** el mismo registro aparecía más de una vez.
2. **Filas sin título o sin sinopsis:** no se pueden buscar ni mostrar.
3. **Duplicados reales:** la misma película (mismo título y año) registrada varias veces, incluso cuando el texto solo se diferenciaba en mayúsculas, acentos o espacios.

Eliminar estas filas evita que la misma película aparezca repetida en los resultados de búsqueda y asegura que cada registro del Trie tenga información completa.

## Estadísticas antes y después

| | Antes | Después |
|---|---|---|
| Películas (filas) | 34,886 | 34,565 |
| Columnas | 8 | 8 |
| Películas eliminadas | | 321 |
| Títulos en mayúsculas | Sí | No |
| Títulos con acentos | Sí | No |
| Nulos en título y sinopsis | Podían existir | 0 |

## Resultado final

El archivo `movies_clean.csv` tiene 34,565 películas y 8 columnas, con títulos en minúsculas, sin acentos, sin espacios extra y sin nulos en las columnas críticas. Está listo para ser cargado por el programa en C++.
