# Código explicado: `main_progra3.cpp`

## 1. ¿Qué hace el programa?

Es una **plataforma de streaming en consola**. Al iniciar, carga las 34,565 películas de `movies_clean.csv` (limpiadas antes en R) y construye un **Trie de sufijos** con los títulos. Después muestra un menú que permite:

- Buscar películas por una palabra, frase o **sub-palabra** del título. Por ejemplo, "bar" encuentra "**bar**ney oldfield's race for a life" y "the **bar**ber of seville".
- Buscar por **director** y por **género**.
- Marcar películas con **"Ver más tarde"** o **"Like"** desde cualquier búsqueda.
- Ver la lista de películas guardadas en "Ver más tarde".

Todo el texto se guarda en minúsculas, así que la búsqueda no distingue mayúsculas de minúsculas.

## 2. Estructuras de datos

### `Movie`
Representa una película. Guarda los 8 campos del CSV: año de estreno, título, origen, director, reparto (cast), género, enlace de Wikipedia y sinopsis.

### `TrieNode`
Es un nodo del árbol. Tiene dos cosas:

- **Hijos:** un mapa que asocia cada carácter (letra, número, espacio o signo) con el nodo siguiente. Cada nodo representa **un solo carácter**.
- **Lista de películas:** los números (índices) de todas las películas cuyo título contiene el texto formado desde la raíz hasta ese nodo.

## 3. La clase `SuffixTrie`

Es la clase principal. Guarda la **raíz** del árbol y el **vector con todas las películas**. Las búsquedas devuelven índices de ese vector, así no se copia la información de cada película dentro del árbol.

### ¿Qué es un Trie de sufijos?

Un **sufijo** es lo que queda de una palabra si le quitas letras del inicio. Los sufijos de "barco" son:

| Sufijo |
|---|
| barco |
| arco |
| rco |
| co |
| o |

Si se insertan **todos** los sufijos de un título en un Trie, cualquier pedazo del título (cualquier *substring*) es el **comienzo** de algún sufijo. Por eso basta con bajar por el árbol desde la raíz para encontrarlo.

### ¿Por qué "bar" encuentra "barco"?

Al insertar el sufijo "barco" se crea el camino raíz → b → a → r → c → o, y en cada nodo de ese camino se anota la película. Al buscar "bar", el programa baja raíz → b → a → r. Ese nodo ya tiene anotada la película, así que la encuentra aunque "bar" sea solo una parte de la palabra.

Con "arco" pasa lo mismo: existe el sufijo "arco", que empieza en la raíz. Así se encuentra una sub-palabra que está en medio del título.

### Ventajas y desventajas

- **Ventaja:** la búsqueda solo depende del largo de lo que se busca, no de cuántas películas hay. Buscar "bar" son 3 pasos, haya 30 mil o 300 mil películas.
- **Desventaja:** usa mucha memoria, porque cada título de *n* letras genera unos *n²/2* nodos. Por eso solo se indexan los títulos (unos 560 MB de RAM y alrededor de 9 segundos de carga).

## 4. Funciones principales

### `loadMovies()`: carga las películas del CSV
Abre el archivo, se salta la fila de encabezados y lee una película a la vez. Para cada una, separa sus campos, la guarda en el vector y le inserta los sufijos del título en el árbol. Cada 5,000 películas muestra el avance. Si no puede abrir el archivo devuelve `false`, y el programa muestra un error y termina.

Dos detalles de la lectura:

- **Registros de varias líneas:** algunos campos entre comillas (el reparto) tienen saltos de línea. `readCSVRecord()` sigue juntando líneas mientras haya comillas sin cerrar, para que cada película se lea completa y no se parta en películas "falsas".
- **Comas y comillas dentro del texto:** en `parseCSVLine()`, una coma dentro de comillas no separa campos, y dos comillas seguidas (`""`) se leen como una comilla normal.

### `insertSuffixes()`: inserta los sufijos en el árbol
Recorre el título letra por letra y, desde cada posición, inserta el sufijo que empieza ahí (`insertSuffix()`). Cada inserción baja por el árbol, crea los nodos que falten y anota el número de la película en cada nodo del camino, sin repetirlo.

### `search()`: busca sub-palabras
Pasa la búsqueda a minúsculas y baja por el árbol carácter por carácter. Si en algún momento no existe el hijo buscado, no hay resultados. Si llega al final, devuelve la lista de películas de ese nodo.

### `searchByDirector()`: busca por director
Recorre todas las películas y se queda con las que tienen el texto buscado dentro del nombre del director. Por ejemplo, "spielberg" encuentra 31 películas.

### `searchByGenre()`: busca por género
Funciona igual que la anterior, pero con el género. Por ejemplo, "anime" encuentra 143 películas.

### `displayResults()`: muestra resultados
Muestra cuántas películas se encontraron y el detalle de **5 películas a partir de una posición** (`desde`): título, año, director, género y los primeros 120 caracteres de la sinopsis. Indica qué parte se está mostrando, por ejemplo "Mostrando 6-10 de 31 resultados".

### Otras funciones
- **`getMovieByIndex()`**: devuelve una película a partir de su número, para la lista de "Ver más tarde".
- **Destructor (`~SuffixTrie`)**: al cerrar el programa recorre el árbol y libera la memoria de todos los nodos.

## 5. Menú principal

Al iniciar, el programa abre `movies_clean.csv`, que debe estar en la misma carpeta que el programa. Luego carga las películas y muestra el menú en un ciclo hasta que el usuario elige salir:

| Opción | Qué hace |
|---|---|
| **1. Buscar película** | Pide una palabra o frase y la busca en el Trie. Si hay resultados, pregunta si se quiere marcar una de las 5 mostradas con "Ver más tarde" o "Like". |
| **2. Buscar por director** | Pide un nombre y muestra las películas de ese director. También permite marcar una con "Ver más tarde" o "Like". |
| **3. Buscar por género** | Pide un género y muestra las películas de ese género. También permite marcar una con "Ver más tarde" o "Like". |
| **4. Ver más tarde** | Lista las películas guardadas. |
| **5. Salir** | Termina el programa. |

Funciones de apoyo del menú:

- **`pausa()`**: después de cada opción espera un Enter para volver al menú principal, así el usuario alcanza a leer los resultados.
- **`leerEntero()`**: si el usuario escribe letras donde se espera un número, lo vuelve a pedir. Con `cin >> numero`, las letras dejaban a `cin` en estado de error y el programa entraba en un bucle infinito.
- **`mostrarPaginado()`**: muestra los resultados **de 5 en 5**. Después de cada grupo permite marcar una película y pregunta "¿Ver las siguientes 5? (s/n)". Se usa en las opciones 1, 2 y 3.
- **`marcarPelicula()`**: pregunta si se quiere marcar una de las 5 películas que están en pantalla con "Ver más tarde" o "Like".
- **`agregarSinRepetir()`**: una película no se puede agregar dos veces a la misma lista.
- Al elegir una película solo se aceptan números que estén en pantalla (por ejemplo, del 1 al 3 si solo hay 3 resultados).

## 6. Pendiente para la entrega final (semana 16)

- Buscar también en la **sinopsis** (el enunciado lo pide) y buscar frases palabra por palabra ("barco fantasma" → "barco" y/o "fantasma").
- Búsqueda por **reparto (cast)**.
- **Algoritmo de importancia** para ordenar los resultados (ahora salen en el orden del CSV).
- Ver la **sinopsis completa** al seleccionar una película.
- Guardar "Ver más tarde" y "Like" entre ejecuciones, y **recomendar películas similares** a las que tienen Like.
