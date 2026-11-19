// Lógica de la interfaz: pide datos al servidor en C++ y los dibuja.

const $ = (id) => document.getElementById(id);

// Estado de la búsqueda actual (para paginar)
let busquedaActual = null;   // { tipo, valor, pagina }
let peliculaAbierta = null;  // id de la película del modal
let numeroPeticion = 0;      // evita que una respuesta lenta pise a una más nueva

const NOMBRES_TAG = { director: "Director", genre: "Género", cast: "Casting", origin: "Origen" };
const SUGERENCIAS = ["ghost ship", "space", "bar", "vampire", "love story", "detective"];

// ---------- Utilidades ----------
async function api(ruta, opciones) {
  const r = await fetch(ruta, opciones);
  const datos = await r.json();
  if (!r.ok && datos.error !== "cargando") throw new Error(datos.error || r.status);
  return datos;
}

function escapar(texto) {
  const d = document.createElement("div");
  d.textContent = texto ?? "";
  return d.innerHTML;
}

// Póster generado: cada título produce siempre el mismo degradado de colores
function colores(titulo) {
  let h = 0;
  for (const c of titulo) h = (h * 31 + c.charCodeAt(0)) >>> 0;
  // Solo tonos cálidos de la marca (rojo vino → ámbar → dorado), para que todo combine
  const h1 = (340 + h % 70) % 360;
  const h2 = (h1 + 340 + (h >> 8) % 30) % 360;
  const luz = 30 + (h >> 4) % 10;
  return `linear-gradient(145deg, hsl(${h1} 58% ${luz}%), hsl(${h2} 60% 12%))`;
}

function primerGenero(genero) {
  if (!genero || genero === "unknown") return "";
  return genero.split(/[,/]/)[0].trim();
}

function mostrarToast(texto) {
  const t = $("toast");
  t.textContent = texto;
  t.classList.add("visible");
  clearTimeout(t._timer);
  t._timer = setTimeout(() => t.classList.remove("visible"), 2200);
}

// ---------- Tarjetas ----------
function crearTarjeta(p, rango) {
  const div = document.createElement("div");
  div.className = "tarjeta";
  const meta = [p.anio || "", primerGenero(p.genero), p.director !== "unknown" ? p.director : ""]
    .filter(Boolean).join(" · ");
  div.innerHTML = `
    <div class="poster" style="background:${colores(p.titulo)}">
      ${rango ? `<span class="rango">#${rango}</span>` : ""}
      <span class="poster-letra">${escapar((p.titulo[0] || "?").toUpperCase())}</span>
      <div class="poster-titulo">${escapar(p.titulo)}</div>
      <div class="poster-anio">${p.anio || ""}</div>
    </div>
    <div class="tarjeta-info">
      <div class="meta">${escapar(meta)}</div>
      <div class="resumen">${escapar(p.sinopsisCorta)}</div>
    </div>`;
  div.tabIndex = 0;
  div.addEventListener("click", () => abrirPelicula(p.id));
  div.addEventListener("keydown", (ev) => { if (ev.key === "Enter") abrirPelicula(p.id); });
  return div;
}

function llenarFila(idContenedor, lista, textoVacio) {
  const cont = $(idContenedor);
  cont.innerHTML = "";
  if (!lista.length) {
    cont.innerHTML = `<div class="vacio">${textoVacio}</div>`;
    return;
  }
  lista.forEach((p) => cont.appendChild(crearTarjeta(p)));
}

// ---------- Inicio ----------
async function cargarInicio() {
  let d;
  try { d = await api("/api/inicio"); } catch { return; }
  llenarFila("fila-ver-mas-tarde", d.verMasTarde, "Aún no guardas películas. Abre una y pulsa “Ver más tarde”.");
  llenarFila("fila-recomendadas", d.recomendadas, "Dale Like a una película y aquí verás otras similares.");
  // El servidor guarda los Likes en orden de llegada: se invierten para mostrar el más reciente primero
  llenarFila("fila-likes", [...d.likes].reverse(), "Todavía no le das Like a ninguna película.");
}

function mostrarInicio() {
  $("vista-resultados").classList.add("oculto");
  $("vista-inicio").classList.remove("oculto");
  busquedaActual = null;
  cargarInicio();
  window.scrollTo({ top: 0, behavior: "smooth" });
}

// ---------- Búsqueda ----------
async function buscar(tipo, valor, pagina = 1) {
  valor = valor.trim();
  if (!valor) return;
  busquedaActual = { tipo, valor, pagina };
  const miPeticion = ++numeroPeticion;

  const url = tipo === "texto"
    ? `/api/buscar?q=${encodeURIComponent(valor)}&pagina=${pagina}`
    : `/api/tag?tipo=${tipo}&valor=${encodeURIComponent(valor)}&pagina=${pagina}`;
  let d;
  try {
    d = await api(url);
  } catch {
    mostrarToast("No se pudo buscar. ¿Sigue abierto el programa?");
    return;
  }
  if (miPeticion !== numeroPeticion) return;  // llegó una búsqueda más nueva
  busquedaActual.pagina = d.pagina || 1;

  $("vista-inicio").classList.add("oculto");
  $("vista-resultados").classList.remove("oculto");

  const etiqueta = tipo === "texto" ? `“${valor}”` : `${NOMBRES_TAG[tipo]}: “${valor}”`;
  $("titulo-resultados").textContent = `Resultados para ${etiqueta}`;
  // Solo la búsqueda por texto usa el algoritmo de importancia; los tags salen en orden del catálogo
  const orden = tipo === "texto" ? "ordenadas por importancia" : "en orden del catálogo";
  $("detalle-resultados").textContent = d.total
    ? `${d.total.toLocaleString("es-PE")} películas · ${orden} · ${d.ms.toFixed(1)} ms`
    : "";

  const grilla = $("grilla-resultados");
  grilla.innerHTML = "";
  if (!d.total) {
    grilla.innerHTML = `<div class="vacio vacio-mascota" style="grid-column:1/-1">
      <img src="img/mascota_3d.svg" alt="">
      <p><strong>¡Ups!</strong> No se encontraron películas.<br>Prueba con otra palabra o parte de palabra.</p>
    </div>`;
  } else {
    const base = (d.pagina - 1) * 5;
    d.resultados.forEach((p, i) => grilla.appendChild(crearTarjeta(p, tipo === "texto" ? base + i + 1 : null)));
  }

  $("texto-pagina").textContent = d.total ? `Página ${d.pagina} de ${d.totalPaginas}` : "";
  document.querySelector(".paginacion").classList.toggle("oculto", !d.total);
  $("btn-anterior").disabled = !d.hayAnterior;
  $("btn-siguiente").disabled = !d.haySiguiente;
  window.scrollTo({ top: 0, behavior: "smooth" });
}

// ---------- Detalle ----------
async function abrirPelicula(id) {
  let p;
  try { p = await api(`/api/pelicula?id=${id}`); } catch { mostrarToast("No se pudo abrir la película."); return; }
  peliculaAbierta = id;

  $("modal-poster").style.background = colores(p.titulo);
  $("modal-titulo").textContent = p.titulo;

  const chips = [p.anio, ...p.genero.split(/[,/]/).map((g) => g.trim()), p.origen]
    .filter((c) => c && c !== "unknown");
  $("modal-chips").innerHTML = chips.map((c) => `<span class="chip">${escapar(String(c))}</span>`).join("");

  $("modal-director").textContent = p.director && p.director !== "unknown" ? p.director : "Desconocido";
  $("modal-casting").textContent = p.casting || "No disponible";
  $("modal-sinopsis").textContent = p.sinopsis;
  $("modal-wiki").href = p.wiki;

  pintarBotones(p.like, p.verMasTarde);
  $("modal").classList.remove("oculto");
  document.body.style.overflow = "hidden";
}

let estadoLike = false, estadoMasTarde = false;  // estado de la película abierta

function pintarBotones(like, masTarde) {
  estadoLike = like;
  estadoMasTarde = masTarde;

  const bl = $("btn-like");
  bl.textContent = like ? "♥ Te gusta" : "♡ Like";
  bl.title = like ? "Clic para quitar el Like" : "";
  bl.classList.toggle("activo", like);

  const bm = $("btn-mas-tarde");
  bm.textContent = masTarde ? "✓ En Ver más tarde" : "＋ Ver más tarde";
  bm.title = masTarde ? "Clic para quitar de Ver más tarde" : "";
  bm.classList.toggle("activo", masTarde);
}

function cerrarModal() {
  if ($("modal").classList.contains("oculto")) return;
  $("modal").classList.add("oculto");
  document.body.style.overflow = "";
  peliculaAbierta = null;
  if (!busquedaActual) cargarInicio();  // refrescar recomendaciones si estamos en inicio
}

async function marcar(tipo) {
  if (peliculaAbierta === null) return;
  // Si ya estaba marcada, el mismo botón la quita
  const quitar = tipo === "like" ? estadoLike : estadoMasTarde;
  const ruta = tipo === "like"
    ? (quitar ? "/api/quitar-like" : "/api/like")
    : (quitar ? "/api/quitar-vermastarde" : "/api/vermastarde");
  let r;
  try { r = await api(`${ruta}?id=${peliculaAbierta}`, { method: "POST" }); }
  catch { mostrarToast("No se pudo guardar. Intenta de nuevo."); return; }
  pintarBotones(r.like, r.verMasTarde);
  const mensajes = {
    like:     ["♥ Agregada a tus Likes · recomendaciones actualizadas", "Like quitado · recomendaciones actualizadas"],
    mastarde: ["✓ Guardada en Ver más tarde", "Quitada de Ver más tarde"],
  };
  mostrarToast(mensajes[tipo][quitar ? 1 : 0]);
}

// ---------- Arranque: esperar a que el árbol esté listo ----------
async function esperarCarga() {
  try {
    const e = await api("/api/estado");
    if (e.error) {
      $("carga-texto").textContent = "Error: no se encontró movies_clean.csv junto al programa.";
      return;
    }
    if (!e.listo) { setTimeout(esperarCarga, 400); return; }

    $("info-carga").textContent = `${e.total.toLocaleString("es-PE")} películas`;
    await cargarInicio();
    $("carga").classList.add("ocultando");
    setTimeout(() => $("carga").classList.add("oculto"), 500);
  } catch {
    $("carga-texto").textContent = "No hay conexión con el servidor. ¿Está abierto el programa?";
    setTimeout(esperarCarga, 1500);
  }
}

// ---------- Eventos ----------
$("form-busqueda").addEventListener("submit", (ev) => {
  ev.preventDefault();
  buscar($("tipo-busqueda").value, $("campo-busqueda").value, 1);
});
$("btn-siguiente").addEventListener("click", () => {
  if (busquedaActual) buscar(busquedaActual.tipo, busquedaActual.valor, busquedaActual.pagina + 1);
});
$("btn-anterior").addEventListener("click", () => {
  if (busquedaActual) buscar(busquedaActual.tipo, busquedaActual.valor, busquedaActual.pagina - 1);
});
$("ir-inicio").addEventListener("click", mostrarInicio);
$("volver-inicio").addEventListener("click", mostrarInicio);
$("btn-like").addEventListener("click", () => marcar("like"));
$("btn-mas-tarde").addEventListener("click", () => marcar("mastarde"));
document.querySelectorAll("[data-cerrar]").forEach((el) => el.addEventListener("click", cerrarModal));
document.addEventListener("keydown", (ev) => { if (ev.key === "Escape") cerrarModal(); });

SUGERENCIAS.forEach((s) => {
  const b = document.createElement("button");
  b.className = "sugerencia";
  b.textContent = s;
  b.addEventListener("click", () => {
    $("tipo-busqueda").value = "texto";
    $("campo-busqueda").value = s;
    buscar("texto", s, 1);
  });
  $("sugerencias").appendChild(b);
});

esperarCarga();
