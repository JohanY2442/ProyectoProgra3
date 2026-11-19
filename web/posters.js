// Pósters de Chestream.
// Como el catálogo no tiene imágenes, cada película recibe un póster con la inicial
// de su título sobre un degradado de tonos cálidos. El color sale de un hash del
// id y el título, así cada película tiene siempre el mismo póster.

const Posters = (() => {
  function hash32(texto) {
    let h = 2166136261;
    for (const c of texto) { h ^= c.charCodeAt(0); h = Math.imul(h, 16777619); }
    return h >>> 0;
  }

  function crear(pelicula) {
    const h = hash32(`${pelicula.id}|${pelicula.titulo}`);
    // Solo tonos de la marca: rojo vino -> ámbar -> dorado
    const tono1 = (340 + h % 70) % 360;
    const tono2 = (tono1 + 340 + (h >> 8) % 30) % 360;
    const luz = 30 + (h >> 4) % 10;
    const inicial = (String(pelicula.titulo || "?").trim()[0] || "?").toUpperCase()
      .replace(/[&<>"']/g, "");
    const uid = `p${pelicula.id}`;

    const svg = `<svg viewBox="0 0 200 300" preserveAspectRatio="xMidYMid slice" aria-hidden="true">
      <defs><linearGradient id="${uid}" x1="0" y1="0" x2="1" y2="1">
        <stop offset="0" stop-color="hsl(${tono1} 58% ${luz}%)"/>
        <stop offset="1" stop-color="hsl(${tono2} 60% 12%)"/>
      </linearGradient></defs>
      <rect width="200" height="300" fill="url(#${uid})"/>
      <text x="184" y="150" text-anchor="end" font-family="Playfair, Georgia, serif" font-weight="900"
            font-size="150" fill="#F6F2E9" opacity=".16">${inicial}</text>
    </svg>`;

    return { svg, familia: "inicial", banda: `hsl(${tono2} 45% 9%)`, tinta: "#F6F2E9" };
  }

  return { crear };
})();
