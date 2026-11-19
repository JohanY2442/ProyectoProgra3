// Lógica de la interfaz de Chestream.
// Solo pide datos al servidor en C++ (mismas rutas /api/...) y los dibuja.

const $ = (sel, el = document) => el.querySelector(sel);
const $$ = (sel, el = document) => [...el.querySelectorAll(sel)];

const estado = {
  busqueda: null,        // { tipo, valor, pagina } de la vista de resultados
  peticion: 0,           // evita que una respuesta lenta pise a una más nueva
  abierta: null,         // id de la película del modal
  likes: new Set(),      // ids con Like (para marcar las tarjetas)
  tarde: new Set(),      // ids en Ver más tarde
};

const NOMBRES_TAG = { director: "Director", genre: "Género", cast: "Casting", origin: "Origen" };
const NOMBRES_LISTA = { likes: "Tus Likes", vermastarde: "Ver más tarde" };
const SUGERENCIAS = ["ghost ship", "space", "bar", "vampire", "love story", "detective"];
const reducirMovimiento = matchMedia("(prefers-reduced-motion: reduce)").matches;

const ICONO = {
  corazon: '<svg viewBox="0 0 24 24" aria-hidden="true"><path d="M12 20s-7-4.4-9.2-9A5 5 0 0 1 12 6a5 5 0 0 1 9.2 5c-2.2 4.6-9.2 9-9.2 9z"/></svg>',
  marcador: '<svg viewBox="0 0 24 24" aria-hidden="true"><path d="M6 3h12v18l-6-4.5L6 21z"/></svg>',
};

// ---------- Red ----------
async function api(ruta, opciones) {
  const r = await fetch(ruta, opciones);
  const datos = await r.json();
  if (!r.ok && datos.error !== "cargando") throw new Error(datos.error || r.status);
  return datos;
}

// ---------- Formato de texto (todo se escapa: anti-XSS) ----------
function esc(t) {
  return String(t ?? "").replace(/[&<>"']/g, (c) =>
    ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" })[c]);
}
function limpiar(t) {
  return String(t ?? "")
    .replace(/\s+/g, " ")
    .replace(/\s+([,.;:!?])/g, "$1")
    .replace(/([.!?])(\p{Ll}{2,})/gu, "$1 $2")   // "crew.the" -> "crew. the" (respeta "u.s.")
    .replace(/,(\p{L})/gu, ", $1")                // "town,called" -> "town, called"
    .trim();
}
// La base viene en minúsculas: mayúscula al inicio de cada oración y en "I"
function oraciones(t) {
  return limpiar(t)
    .replace(/(^|[.!?]\s+)(["'(“]?)(\p{Ll})/gu, (m, a, q, l) => a + q + l.toUpperCase())
    .replace(/\bi\b/g, "I")
    .replace(/\.\.\.$/, "…");
}
const MENORES = new Set("a an and as at but by for from in into of on or the to vs with de del la el los las y en".split(" "));
function enTitulo(t) {
  const palabras = limpiar(t).split(" ");
  return palabras.map((p, i) =>
    (i > 0 && MENORES.has(p)) ? p : p.replace(/^([("'¿¡“]*)(\p{Ll})/u, (m, a, l) => a + l.toUpperCase())
  ).join(" ");
}
// Lista de nombres sin repetidos ni "unknown" (directores, elenco)
function nombres(t) {
  const vistos = new Set(), lista = [];
  for (const parte of limpiar(t).split(/\s*[,;]\s*|\s+and\s+/i)) {
    const p = parte.trim();
    if (!p || p.toLowerCase() === "unknown" || vistos.has(p.toLowerCase())) continue;
    vistos.add(p.toLowerCase());
    lista.push(enTitulo(p));
  }
  return lista;
}
function generos(t) {
  return [...new Set(limpiar(t).split(/\s*[,/]\s*/).map((g) => g.trim())
    .filter((g) => g && g !== "unknown"))].map((g) => g[0].toUpperCase() + g.slice(1));
}
const numero = (n) => Number(n).toLocaleString("es-PE");

// Resaltado de coincidencias sobre texto ya escapado (sin romper entidades)
function terminos(q) {
  return [...new Set((q.toLowerCase().match(/[a-z0-9]+/g) || []))].sort((a, b) => b.length - a.length);
}
function resaltar(texto, lista) {
  const seguro = esc(texto);
  if (!lista || !lista.length) return seguro;
  const re = new RegExp(`(${lista.join("|")})`, "gi");
  return seguro.replace(/(&[a-z#0-9]+;)|([^&]+)/gi, (m, ent, txt) => ent || txt.replace(re, "<mark>$1</mark>"));
}

// ---------- Toasts ----------
function toast(texto) {
  const t = document.createElement("div");
  t.className = "toast";
  t.textContent = texto;
  $("#toasts").appendChild(t);
  requestAnimationFrame(() => t.classList.add("visible"));
  setTimeout(() => { t.classList.remove("visible"); setTimeout(() => t.remove(), 300); }, 2400);
}

// ---------- Póster ----------
function htmlPoster(p, opciones = {}) {
  const post = Posters.crear(p);
  const genero = generos(p.genero)[0] || "";
  const director = nombres(p.director)[0];
  const titulo = opciones.terminos ? resaltar(enTitulo(p.titulo), opciones.terminos) : esc(enTitulo(p.titulo));
  return `
    <div class="poster poster-${post.familia}" style="--p-banda:${post.banda};--p-tinta:${post.tinta}">
      <div class="poster-arte">${post.svg}</div>
      <div class="poster-sup">${genero ? `<span class="poster-genero">${esc(genero)}</span>` : "<span></span>"}</div>
      <div class="poster-banda">
        <span class="poster-anio">${p.anio || ""}</span>
        <h3 class="poster-titulo">${titulo}</h3>
        <span class="poster-dir">${director ? "Dir. " + esc(director) : "&nbsp;"}</span>
      </div>
    </div>`;
}

// ---------- Tarjetas ----------
function crearTarjeta(p, opciones = {}) {
  const art = document.createElement("article");
  art.className = "peli";
  art.tabIndex = 0;
  art.dataset.id = p.id;
  art.setAttribute("aria-label", `${enTitulo(p.titulo)}, ${p.anio}`);
  if (opciones.indice !== undefined) art.style.setProperty("--i", opciones.indice);

  const sinopsis = opciones.terminos ? resaltar(oraciones(p.sinopsisCorta), opciones.terminos) : esc(oraciones(p.sinopsisCorta));
  art.innerHTML = `
    ${htmlPoster(p, opciones)}
    <div class="rapidas">
      <button class="rapida rapida-like" data-accion="like" aria-label="Like">${ICONO.corazon}</button>
      <button class="rapida rapida-tarde" data-accion="tarde" aria-label="Ver más tarde">${ICONO.marcador}</button>
    </div>
    <div class="peli-pie">
      ${opciones.rango ? `<span class="peli-rango" title="Puesto en los resultados">${opciones.rango}</span>` : ""}
      <p class="peli-sinopsis">${sinopsis}</p>
    </div>`;

  art.addEventListener("click", () => abrirPelicula(p.id));
  art.addEventListener("keydown", (ev) => { if (ev.key === "Enter" && ev.target === art) abrirPelicula(p.id); });
  $$(".rapida", art).forEach((b) => b.addEventListener("click", (ev) => {
    ev.stopPropagation();                       // no abrir el modal
    alternar(b.dataset.accion, p.id);
  }));
  pintarMarcas(art, p.id);
  return art;
}

function pintarMarcas(raiz, id) {
  const like = estado.likes.has(id), tarde = estado.tarde.has(id);
  const bl = $(".rapida-like", raiz), bt = $(".rapida-tarde", raiz);
  if (bl) { bl.classList.toggle("activa", like); bl.setAttribute("aria-pressed", like); bl.setAttribute("aria-label", like ? "Quitar Like" : "Dar Like"); }
  if (bt) { bt.classList.toggle("activa", tarde); bt.setAttribute("aria-pressed", tarde); bt.setAttribute("aria-label", tarde ? "Quitar de Ver más tarde" : "Guardar para ver más tarde"); }
}

function refrescarMarcas(id) {
  $$(`.peli[data-id="${id}"]`).forEach((el) => pintarMarcas(el, id));
  if (estado.abierta === id) pintarBotonesModal();
  $("#contador-likes").textContent = estado.likes.size;
  $("#contador-tarde").textContent = estado.tarde.size;
}

// ---------- Like / Ver más tarde (actualización optimista) ----------
let temporizadorInicio = null;
async function alternar(tipo, id) {
  const conjunto = tipo === "like" ? estado.likes : estado.tarde;
  const quitar = conjunto.has(id);
  quitar ? conjunto.delete(id) : conjunto.add(id);
  refrescarMarcas(id);

  const ruta = tipo === "like"
    ? (quitar ? "/api/quitar-like" : "/api/like")
    : (quitar ? "/api/quitar-vermastarde" : "/api/vermastarde");
  try {
    const r = await api(`${ruta}?id=${id}`, { method: "POST" });
    r.like ? estado.likes.add(id) : estado.likes.delete(id);
    r.verMasTarde ? estado.tarde.add(id) : estado.tarde.delete(id);
    refrescarMarcas(id);
    const textos = {
      like: ["Agregada a tus Likes", "Like quitado"],
      tarde: ["Guardada en Ver más tarde", "Quitada de Ver más tarde"],
    };
    toast(textos[tipo][quitar ? 1 : 0]);
  } catch {
    quitar ? conjunto.add(id) : conjunto.delete(id);   // deshacer
    refrescarMarcas(id);
    toast("No se pudo guardar. ¿Sigue abierto el programa?");
    return;
  }
  // Las recomendaciones cambian con cada Like: refrescar el inicio sin parpadeos
  clearTimeout(temporizadorInicio);
  temporizadorInicio = setTimeout(() => { if (!estado.busqueda) cargarInicio(); }, 600);
}

// ---------- Inicio ----------
function llenarCarril(idCont, lista, vacio) {
  const cont = $("#" + idCont);
  cont.innerHTML = "";
  if (!lista.length) {
    cont.innerHTML = `
      <div class="vacio">
        <img src="img/mascota_3d.svg" alt="">
        <div><p>${vacio.texto}</p>
        <button class="btn-fantasma vacio-accion">${vacio.accion}</button></div>
      </div>`;
    $(".vacio-accion", cont).addEventListener("click", enfocarBuscador);
    return;
  }
  lista.forEach((p) => cont.appendChild(crearTarjeta(p)));
}

async function cargarInicio() {
  let d;
  try { d = await api("/api/inicio"); } catch { return; }
  estado.likes = new Set(d.likes.map((p) => p.id));
  estado.tarde = new Set(d.verMasTarde.map((p) => p.id));
  $("#contador-likes").textContent = estado.likes.size;
  $("#contador-tarde").textContent = estado.tarde.size;

  $("#nota-recom").textContent = d.likes.length
    ? `A partir de ${d.likes.length === 1 ? "tu Like" : `tus ${d.likes.length} Likes`}` : "";
  llenarCarril("fila-recomendadas", d.recomendadas,
    { texto: "Dale Like a una película y aquí aparecerán otras parecidas.", accion: "Buscar una película" });
  llenarCarril("fila-ver-mas-tarde", d.verMasTarde,
    { texto: "Guarda películas para verlas después. Aparecerán aquí al abrir Chestream.", accion: "Buscar una película" });
  llenarCarril("fila-likes", [...d.likes].reverse(),
    { texto: "Todavía no le das Like a ninguna película.", accion: "Buscar una película" });
}

function mostrarInicio() {
  $("#vista-resultados").classList.add("oculto");
  $("#vista-inicio").classList.remove("oculto");
  estado.busqueda = null;
  cargarInicio();
  window.scrollTo({ top: 0, behavior: reducirMovimiento ? "auto" : "smooth" });
}

// ---------- Búsqueda y listas ----------
async function buscar(tipo, valor, pagina = 1) {
  valor = String(valor).trim();
  if (!valor) return;
  estado.busqueda = { tipo, valor, pagina };
  const mia = ++estado.peticion;

  let url;
  if (tipo === "texto") url = `/api/buscar?q=${encodeURIComponent(valor)}&pagina=${pagina}`;
  else if (tipo === "lista") url = `/api/lista?tipo=${valor}&todos=1`;
  else url = `/api/tag?tipo=${tipo}&valor=${encodeURIComponent(valor)}&pagina=${pagina}`;

  let d;
  try { d = await api(url); } catch { toast("No se pudo buscar. ¿Sigue abierto el programa?"); return; }
  if (mia !== estado.peticion) return;
  estado.busqueda.pagina = d.pagina || 1;

  $("#vista-inicio").classList.add("oculto");
  $("#vista-resultados").classList.remove("oculto");

  // Título y número de películas encontradas
  if (tipo === "lista") $("#titulo-resultados").textContent = NOMBRES_LISTA[valor];
  else $("#titulo-resultados").textContent = tipo === "texto"
    ? `Resultados para “${valor}”` : `${NOMBRES_TAG[tipo]}: “${valor}”`;
  $("#conteo-resultados").textContent = d.total ? `${numero(d.total)} ${d.total === 1 ? "película" : "películas"}` : "";

  const grilla = $("#grilla-resultados");
  grilla.innerHTML = "";
  grilla.classList.remove("animar");
  void grilla.offsetWidth;                         // reiniciar la animación de entrada
  if (!reducirMovimiento) grilla.classList.add("animar");

  if (!d.total) {
    grilla.innerHTML = tipo === "lista"
      ? `<div class="vacio vacio-ancho"><img src="img/mascota_3d.svg" alt=""><div><p>Esta lista está vacía.</p>
           <button class="btn-fantasma vacio-accion">Buscar una película</button></div></div>`
      : `<div class="vacio vacio-ancho"><img src="img/mascota_3d.svg" alt=""><div>
           <p><strong>No hay películas con “${esc(valor)}”.</strong><br>Prueba con una parte de la palabra o busca por director o casting.</p>
           <button class="btn-fantasma vacio-accion">Probar otra búsqueda</button></div></div>`;
    $(".vacio-accion", grilla).addEventListener("click", enfocarBuscador);
  } else {
    const lista = tipo === "texto" ? terminos(valor) : null;
    const base = (d.pagina - 1) * 5;
    d.resultados.forEach((p, i) => grilla.appendChild(crearTarjeta(p, {
      indice: i, rango: tipo === "texto" ? base + i + 1 : null, terminos: lista,
    })));
  }

  const pag = $(".paginacion");
  pag.classList.toggle("oculto", !d.total || tipo === "lista");
  $("#texto-pagina").innerHTML = d.total ? `Página <b>${numero(d.pagina)}</b> de ${numero(d.totalPaginas)}` : "";
  $("#btn-anterior").disabled = !d.hayAnterior;
  $("#btn-siguiente").disabled = !d.haySiguiente;
  window.scrollTo({ top: 0, behavior: reducirMovimiento ? "auto" : "smooth" });
}

function paginar(delta) {
  const b = estado.busqueda;
  if (!b || b.tipo === "lista") return;
  if (delta < 0 && $("#btn-anterior").disabled) return;
  if (delta > 0 && $("#btn-siguiente").disabled) return;
  buscar(b.tipo, b.valor, b.pagina + delta);
}

// ---------- Detalle ----------
function pill(texto, tipo, valor) {
  return `<button class="pill" data-tipo="${tipo}" data-valor="${esc(valor)}" title="Buscar por ${NOMBRES_TAG[tipo].toLowerCase()}">${esc(texto)}</button>`;
}

async function abrirPelicula(id) {
  let p;
  try { p = await api(`/api/pelicula?id=${id}`); } catch { toast("No se pudo abrir la película."); return; }
  estado.abierta = id;
  p.like ? estado.likes.add(id) : estado.likes.delete(id);
  p.verMasTarde ? estado.tarde.add(id) : estado.tarde.delete(id);

  const termin = estado.busqueda && estado.busqueda.tipo === "texto" ? terminos(estado.busqueda.valor) : null;
  $("#modal-poster").innerHTML = htmlPoster(p);
  $("#modal-titulo").textContent = enTitulo(p.titulo);

  const listaGen = generos(p.genero);
  const meta = [p.anio, p.origen && p.origen !== "unknown" ? enTitulo(p.origen) : "", ...listaGen].filter(Boolean);
  $("#modal-meta").innerHTML = meta.map((m) => `<span>${esc(String(m))}</span>`).join("");

  const parrafos = oraciones(p.sinopsis).match(/[^.!?]+[.!?]+["')”]?\s*|[^.!?]+$/g) || [""];
  // Agrupar oraciones en párrafos de ~3 para mejor ritmo de lectura
  const bloques = [];
  for (let i = 0; i < parrafos.length; i += 3) bloques.push(parrafos.slice(i, i + 3).join(" ").trim());
  $("#modal-sinopsis").innerHTML = bloques.map((b) => `<p>${resaltar(b, termin)}</p>`).join("");

  const directores = nombres(p.director);
  const elenco = nombres(p.casting);
  const visibles = elenco.slice(0, 8);
  let creditos = "";
  if (directores.length) creditos += `<div class="credito"><span class="credito-rotulo">Dirección</span><div>${directores.map((d) => pill(d, "director", d)).join("")}</div></div>`;
  if (visibles.length) creditos += `<div class="credito"><span class="credito-rotulo">Reparto</span><div>${visibles.map((a) => pill(a, "cast", a)).join("")}${elenco.length > 8 ? `<span class="pill-mas">y ${elenco.length - 8} más</span>` : ""}</div></div>`;
  if (listaGen.length) creditos += `<div class="credito"><span class="credito-rotulo">Género</span><div>${listaGen.map((g) => pill(g, "genre", g)).join("")}</div></div>`;
  if (p.origen && p.origen !== "unknown") creditos += `<div class="credito"><span class="credito-rotulo">Origen</span><div>${pill(enTitulo(p.origen), "origin", p.origen)}</div></div>`;
  $("#modal-creditos").innerHTML = creditos;
  $$("#modal-creditos .pill").forEach((b) => b.addEventListener("click", () => {
    cerrarModal();
    $("#tipo-busqueda").value = b.dataset.tipo;
    $("#campo-busqueda").value = b.dataset.valor;
    actualizarLimpiar();
    buscar(b.dataset.tipo, b.dataset.valor, 1);
  }));
  $("#modal-wiki").href = p.wiki;

  pintarBotonesModal();
  $("#modal").classList.remove("oculto");
  document.body.style.overflow = "hidden";
  $(".modal-caja").scrollTop = 0;
  $("#btn-like").focus({ preventScroll: true });
}

function pintarBotonesModal() {
  const id = estado.abierta;
  const like = estado.likes.has(id), tarde = estado.tarde.has(id);
  const bl = $("#btn-like"), bt = $("#btn-mas-tarde");
  bl.innerHTML = `${ICONO.corazon}<span>${like ? "Te gusta" : "Like"}</span>`;
  bt.innerHTML = `${ICONO.marcador}<span>${tarde ? "En Ver más tarde" : "Ver más tarde"}</span>`;
  bl.classList.toggle("activo", like); bl.setAttribute("aria-pressed", like);
  bt.classList.toggle("activo", tarde); bt.setAttribute("aria-pressed", tarde);
  bl.title = like ? "Clic para quitar el Like" : "";
  bt.title = tarde ? "Clic para quitar de Ver más tarde" : "";
}

function cerrarModal() {
  if ($("#modal").classList.contains("oculto")) return;
  $("#modal").classList.add("oculto");
  document.body.style.overflow = "";
  const id = estado.abierta;
  estado.abierta = null;
  if (!estado.busqueda) cargarInicio();
  else if (estado.busqueda.tipo === "lista") buscar("lista", estado.busqueda.valor, 1);
  const tarjeta = $(`.peli[data-id="${id}"]`);
  if (tarjeta) tarjeta.focus({ preventScroll: true });
}

// ---------- Buscador ----------
function enfocarBuscador() {
  const c = $("#campo-busqueda");
  c.focus();
  c.select();
}
function actualizarLimpiar() {
  $("#btn-limpiar").classList.toggle("oculto", !$("#campo-busqueda").value);
}

// ---------- Arranque: esperar a que el árbol esté listo ----------
async function esperarCarga() {
  try {
    const e = await api("/api/estado");
    if (e.error) { $("#carga-texto").textContent = "No se encontró el catálogo de películas."; return; }
    if (!e.listo) { setTimeout(esperarCarga, 400); return; }

    $("#info-carga").textContent = `${numero(e.total)} películas`;
    await cargarInicio();
    const splash = $("#carga");
    splash.classList.add("saliendo");
    document.body.classList.remove("cargando");
    setTimeout(() => splash.remove(), reducirMovimiento ? 0 : 700);
  } catch {
    $("#carga-texto").textContent = "No hay conexión con el programa. ¿Está abierto?";
    setTimeout(esperarCarga, 1500);
  }
}

// ---------- Eventos ----------
$("#form-busqueda").addEventListener("submit", (ev) => {
  ev.preventDefault();
  buscar($("#tipo-busqueda").value, $("#campo-busqueda").value, 1);
  $("#campo-busqueda").blur();
});
$("#campo-busqueda").addEventListener("input", actualizarLimpiar);
$("#btn-limpiar").addEventListener("click", () => { $("#campo-busqueda").value = ""; actualizarLimpiar(); enfocarBuscador(); });
$("#btn-siguiente").addEventListener("click", () => paginar(1));
$("#btn-anterior").addEventListener("click", () => paginar(-1));
$("#ir-inicio").addEventListener("click", (ev) => { ev.preventDefault(); mostrarInicio(); });
$("#volver-inicio").addEventListener("click", mostrarInicio);
$$("[data-lista]").forEach((b) => b.addEventListener("click", () => buscar("lista", b.dataset.lista, 1)));
$("#btn-like").addEventListener("click", () => estado.abierta !== null && alternar("like", estado.abierta));
$("#btn-mas-tarde").addEventListener("click", () => estado.abierta !== null && alternar("tarde", estado.abierta));
$$("[data-cerrar]").forEach((el) => el.addEventListener("click", cerrarModal));

document.addEventListener("keydown", (ev) => {
  const escribiendo = ["INPUT", "SELECT", "TEXTAREA"].includes(document.activeElement.tagName);
  const modalAbierto = !$("#modal").classList.contains("oculto");
  if ((ev.key === "/" && !escribiendo) || (ev.key.toLowerCase() === "k" && (ev.ctrlKey || ev.metaKey))) {
    ev.preventDefault();
    if (!modalAbierto) enfocarBuscador();
  } else if (ev.key === "Escape") {
    if (modalAbierto) cerrarModal();
    else if (escribiendo) document.activeElement.blur();
  } else if (!escribiendo && !modalAbierto && (ev.key === "ArrowLeft" || ev.key === "ArrowRight")) {
    paginar(ev.key === "ArrowRight" ? 1 : -1);
  }
});

SUGERENCIAS.forEach((s) => {
  const b = document.createElement("button");
  b.className = "sugerencia";
  b.textContent = s;
  b.addEventListener("click", () => {
    $("#tipo-busqueda").value = "texto";
    $("#campo-busqueda").value = s;
    actualizarLimpiar();
    buscar("texto", s, 1);
  });
  $("#sugerencias").appendChild(b);
});

esperarCarga();
