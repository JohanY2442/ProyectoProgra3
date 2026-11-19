// ============================================================================
//  clean_movies.cpp
//  Limpia wiki_movie_plots_deduped.csv y genera un CSV limpio (UTF-8).
//
//  Compilar:
//    g++ -O2 -std=c++17 -o clean_movies clean_movies.cpp
//    (MSVC)  cl /O2 /std:c++17 /EHsc clean_movies.cpp
//
//  Usar:
//    ./clean_movies [entrada.csv] [salida.csv] [--keep-case] [--min-plot N] [--bom]
//
//    entrada.csv   por defecto: wiki_movie_plots_deduped.csv
//    salida.csv    por defecto: movies_clean_1_.csv
//    --keep-case   NO pasa a minúsculas Title/Director/Cast/Plot
//                  (por defecto SÍ, igual que movies_clean_1_.csv)
//    --min-plot N  descarta tramas con menos de N bytes (por defecto 25)
//    --bom         escribe BOM UTF-8 (útil si vas a abrirlo con Excel en Windows)
//
//  Además de la salida se genera <salida>_descartadas.csv con cada fila
//  eliminada y el motivo, para que nada desaparezca sin dejar rastro.
// ============================================================================
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using std::string;
using std::vector;

static const size_t NPOS = string::npos;
static const int MIN_YEAR = 1888;   // primera película registrada
static const int MAX_YEAR = 2026;

struct Options {
    string in = "wiki_movie_plots_deduped.csv";
    string out = "movies_clean_1_.csv";
    bool lower = true;
    size_t min_plot = 25;
    bool bom = false;
};

// ----------------------------------------------------------------------------
//  Contadores para el resumen final
// ----------------------------------------------------------------------------
struct Stats {
    size_t read = 0, written = 0;
    size_t rej_cols = 0, rej_year = 0, rej_title = 0, rej_plot_empty = 0,
           rej_plot_short = 0, rej_plot_trunc = 0, rej_dup = 0;
    size_t refs_removed = 0, prefix_fixed = 0, cast_garbage = 0, genre_junk = 0,
           genre_leak = 0, origin_fixed = 0, bad_url = 0, title_suffix = 0;
};
static Stats G;

// ----------------------------------------------------------------------------
//  Utilidades de texto básicas
// ----------------------------------------------------------------------------
static string lower_ascii(string s) {
    for (auto& ch : s) if (ch >= 'A' && ch <= 'Z') ch = (char)(ch + 32);
    return s;
}
static string trim(const string& s) {
    size_t a = 0, b = s.size();
    while (a < b && s[a] == ' ') ++a;
    while (b > a && s[b - 1] == ' ') --b;
    return s.substr(a, b - a);
}
static string trim_edges(const string& s, const string& chars) {
    size_t a = 0, b = s.size();
    while (a < b && chars.find(s[a]) != NPOS) ++a;
    while (b > a && chars.find(s[b - 1]) != NPOS) --b;
    return s.substr(a, b - a);
}
static bool starts_with(const string& s, const string& p) {
    return s.size() >= p.size() && s.compare(0, p.size(), p) == 0;
}
static bool ends_with(const string& s, const string& p) {
    return s.size() >= p.size() && s.compare(s.size() - p.size(), p.size(), p) == 0;
}
static size_t word_count(const string& s) {
    size_t n = 0; bool in = false;
    for (char c : s) {
        if (c == ' ') in = false;
        else if (!in) { in = true; ++n; }
    }
    return n;
}
static bool has_letter(const string& s) {
    for (unsigned char c : s)
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c >= 0x80) return true;
    return false;
}
static bool has_digit(const string& s) {
    for (char c : s) if (c >= '0' && c <= '9') return true;
    return false;
}
static bool is_alnum_byte(unsigned char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c >= 0x80;
}
static string replace_all(string s, const string& from, const string& to) {
    size_t p = 0;
    while ((p = s.find(from, p)) != NPOS) { s.replace(p, from.size(), to); p += to.size(); }
    return s;
}
// Reemplaza 'from' solo cuando es una palabra completa
static string replace_word(string s, const string& from, const string& to) {
    size_t p = 0;
    while ((p = s.find(from, p)) != NPOS) {
        bool left_ok = (p == 0) || !is_alnum_byte((unsigned char)s[p - 1]);
        size_t e = p + from.size();
        bool right_ok = (e >= s.size()) || !is_alnum_byte((unsigned char)s[e]);
        if (left_ok && right_ok) { s.replace(p, from.size(), to); p += to.size(); }
        else p = e;
    }
    return s;
}
static bool contains_word_seq(const string& hay, const string& needle) {
    if (needle.empty()) return false;
    size_t p = 0;
    while ((p = hay.find(needle, p)) != NPOS) {
        bool l = (p == 0) || !is_alnum_byte((unsigned char)hay[p - 1]);
        size_t e = p + needle.size();
        bool r = (e >= hay.size()) || !is_alnum_byte((unsigned char)hay[e]);
        if (l && r) return true;
        ++p;
    }
    return false;
}

// ----------------------------------------------------------------------------
//  UTF-8: decodificación validada, minúsculas y normalización
// ----------------------------------------------------------------------------
static bool next_cp(const string& s, size_t& i, uint32_t& cp) {
    unsigned char c = (unsigned char)s[i];
    if (c < 0x80) { cp = c; ++i; return true; }
    int len; uint32_t v;
    if ((c & 0xE0) == 0xC0) { len = 2; v = c & 0x1F; }
    else if ((c & 0xF0) == 0xE0) { len = 3; v = c & 0x0F; }
    else if ((c & 0xF8) == 0xF0) { len = 4; v = c & 0x07; }
    else { ++i; return false; }
    if (i + (size_t)len > s.size()) { ++i; return false; }
    for (int k = 1; k < len; ++k) {
        unsigned char cc = (unsigned char)s[i + k];
        if ((cc & 0xC0) != 0x80) { ++i; return false; }
        v = (v << 6) | (cc & 0x3F);
    }
    if ((len == 2 && v < 0x80) || (len == 3 && v < 0x800) ||
        (len == 4 && (v < 0x10000 || v > 0x10FFFF)) || (v >= 0xD800 && v <= 0xDFFF)) {
        ++i; return false;
    }
    i += len; cp = v; return true;
}
static void append_cp(string& out, uint32_t cp) {
    if (cp < 0x80) out += (char)cp;
    else if (cp < 0x800) {
        out += (char)(0xC0 | (cp >> 6));
        out += (char)(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += (char)(0xE0 | (cp >> 12));
        out += (char)(0x80 | ((cp >> 6) & 0x3F));
        out += (char)(0x80 | (cp & 0x3F));
    } else {
        out += (char)(0xF0 | (cp >> 18));
        out += (char)(0x80 | ((cp >> 12) & 0x3F));
        out += (char)(0x80 | ((cp >> 6) & 0x3F));
        out += (char)(0x80 | (cp & 0x3F));
    }
}
// Minúsculas para ASCII, Latin-1, Latin Extendido-A, griego y cirílico básicos
static uint32_t lower_cp(uint32_t c) {
    if (c < 0x80) return (c >= 'A' && c <= 'Z') ? c + 32 : c;
    if (c >= 0xC0 && c <= 0xDE && c != 0xD7) return c + 32;
    if (c >= 0x100 && c <= 0x17F) {
        if (c == 0x130) return 'i';
        if (c == 0x178) return 0xFF;
        if (c == 0x138 || c == 0x149 || c == 0x17F) return c;
        if ((c >= 0x139 && c <= 0x148) || (c >= 0x179 && c <= 0x17E)) return (c % 2 == 1) ? c + 1 : c;
        return (c % 2 == 0) ? c + 1 : c;
    }
    if (c >= 0x391 && c <= 0x3A9 && c != 0x3A2) return c + 32;
    if (c >= 0x410 && c <= 0x42F) return c + 32;
    if (c >= 0x400 && c <= 0x40F) return c + 80;
    return c;
}

// Normaliza: UTF-8 válido, sin caracteres de control, espacios "raros" -> ' ',
// comillas/guiones tipográficos -> ASCII, y opcionalmente minúsculas.
static string normalize_text(const string& in, bool lower) {
    string out; out.reserve(in.size());
    size_t i = 0; uint32_t cp;
    while (i < in.size()) {
        if (!next_cp(in, i, cp)) continue;                       // byte inválido: se descarta
        if (cp == '\t' || cp == '\r' || cp == '\n' || cp == 0x0B || cp == 0x0C ||
            cp == 0x85 || cp == 0x2028 || cp == 0x2029) cp = ' ';
        else if (cp < 0x20 || cp == 0x7F || (cp >= 0x80 && cp < 0xA0)) continue;
        if (cp == 0xA0 || cp == 0x1680 || (cp >= 0x2000 && cp <= 0x200A) ||
            cp == 0x202F || cp == 0x205F || cp == 0x3000) cp = ' ';
        else if (cp == 0xAD || (cp >= 0x200B && cp <= 0x200F) || (cp >= 0x202A && cp <= 0x202E) ||
                 cp == 0x2060 || cp == 0xFEFF || cp == 0xFFFD) continue;
        if (cp == 0x2018 || cp == 0x2019 || cp == 0x201A || cp == 0x201B ||
            cp == 0x2032 || cp == 0x02BC || cp == 0xB4) cp = '\'';
        else if (cp == 0x201C || cp == 0x201D || cp == 0x201E || cp == 0x201F || cp == 0x2033) cp = '"';
        else if ((cp >= 0x2010 && cp <= 0x2015) || cp == 0x2212) cp = '-';
        else if (cp == 0x2026) { out += "..."; continue; }
        if (lower) cp = lower_cp(cp);
        append_cp(out, cp);
    }
    return out;
}
static string squeeze(const string& s) {
    string out; out.reserve(s.size());
    bool prev_space = true;
    for (char c : s) {
        if (c == ' ') { if (!prev_space) out += ' '; prev_space = true; }
        else { out += c; prev_space = false; }
    }
    if (!out.empty() && out.back() == ' ') out.pop_back();
    return out;
}

// ----------------------------------------------------------------------------
//  Restos de Wikipedia: etiquetas HTML y notas [1], [citation needed]...
// ----------------------------------------------------------------------------
static bool ci_match(const string& s, size_t pos, const char* lit) {
    for (size_t k = 0; lit[k]; ++k) {
        if (pos + k >= s.size()) return false;
        char c = s[pos + k];
        if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
        if (c != lit[k]) return false;
    }
    return true;
}
static size_t ci_find(const string& s, const char* lit, size_t from) {
    for (size_t p = from; p < s.size(); ++p) if (ci_match(s, p, lit)) return p;
    return NPOS;
}
static bool is_ascii_alpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }

static string strip_html(const string& s) {
    if (s.find('<') == NPOS) return s;
    string out; out.reserve(s.size());
    size_t i = 0, n = s.size();
    while (i < n) {
        if (s[i] == '<') {
            // <ref ...> ... </ref>  (se elimina junto con su contenido)
            if (ci_match(s, i, "<ref") && i + 4 < n && (s[i + 4] == '>' || s[i + 4] == ' ' || s[i + 4] == '/')) {
                size_t gt = s.find('>', i);
                if (gt != NPOS && gt - i <= 200) {
                    if (s[gt - 1] == '/') { i = gt + 1; ++G.refs_removed; continue; }
                    size_t close = ci_find(s, "</ref>", gt + 1);
                    if (close != NPOS && close - gt <= 1500) { i = close + 6; ++G.refs_removed; continue; }
                    i = gt + 1; continue;
                }
            }
            // cualquier otra etiqueta HTML (<br>, </ref>, <i>...). Los "<Анна>" no se tocan.
            size_t k = i + 1;
            if (k < n && s[k] == '/') ++k;
            if (k < n && is_ascii_alpha(s[k])) {
                size_t gt = s.find('>', k);
                if (gt != NPOS && gt - i <= 200) { i = gt + 1; continue; }
            }
        }
        out += s[i++];
    }
    return out;
}

static bool is_ref_note(const string& inner, bool attached) {
    string c = lower_ascii(trim(inner));
    if (c.empty()) return true;
    bool digits = true;
    for (char ch : c) if (ch < '0' || ch > '9') digits = false;
    if (digits) return true;                                          // [1] [23]
    if (attached && c.size() == 1 && c[0] >= 'a' && c[0] <= 'd') return true;   // palabra[a]
    if (c == "nb") return true;
    static const char* notes[] = {"note ", "notes ", "n ", "nb "};
    for (const char* pre : notes) {                                   // [note 1] [n 2]
        string p(pre);
        if (starts_with(c, p)) {
            string rest = trim(c.substr(p.size()));
            if (!rest.empty() && rest.size() <= 3) return true;
        }
    }
    if (ends_with(c, "needed") || ends_with(c, "needed?")) return true;   // [citation needed]...
    static const char* exact[] = {"who?", "when?", "where?", "why?", "how?", "which?", "what?",
                                  "according to whom?", "original research?", "original research",
                                  "vague", "dubious", "discuss", "citation", "clarify"};
    for (const char* e : exact) if (c == e) return true;
    static const char* prefixes[] = {"dubious", "self-published", "not in citation", "failed verification",
                                     "unreliable source", "citation"};
    for (const char* p : prefixes) if (starts_with(c, p)) return true;
    return false;
}

// Elimina las notas de Wikipedia. Si unwrap_other = true (Director, Cast, Genre),
// los corchetes que NO son nota se quitan pero se conserva el texto: "[Haruma Miura]".
// En Title y Plot los corchetes que no son nota se dejan intactos ("[REC]", "[her]").
static string strip_brackets(const string& s, bool unwrap_other) {
    if (s.find('[') == NPOS && s.find(']') == NPOS) return s;
    string out; out.reserve(s.size());
    size_t i = 0, n = s.size();
    while (i < n) {
        char c = s[i];
        if (c == '[') {
            size_t j = i + 1;
            while (j < n && s[j] != ']' && s[j] != '[' && j - i <= 80) ++j;
            if (j < n && s[j] == ']') {
                string inner = s.substr(i + 1, j - i - 1);
                bool attached = !out.empty() && out.back() != ' ';
                if (is_ref_note(inner, attached)) {
                    if (!out.empty() && out.back() == ' ' && j + 1 < n &&
                        string(".,;:!?)").find(s[j + 1]) != NPOS) out.pop_back();
                    i = j + 1; ++G.refs_removed; continue;
                }
                if (unwrap_other) { out += inner; i = j + 1; continue; }
            } else if (unwrap_other) { ++i; continue; }           // '[' suelto
        } else if (c == ']' && unwrap_other) { ++i; continue; }   // ']' suelto
        out += c; ++i;
    }
    return out;
}

// Quita "( ... )" completos (premios, "(as Joe)", "(producer)"...)
static string remove_parens(const string& s) {
    if (s.find('(') == NPOS && s.find(')') == NPOS) return s;
    string out; out.reserve(s.size());
    size_t i = 0, n = s.size();
    while (i < n) {
        if (s[i] == '(') {
            int depth = 0; size_t j = i;
            for (; j < n && j - i <= 200; ++j) {
                if (s[j] == '(') ++depth;
                else if (s[j] == ')') { if (--depth == 0) break; }
            }
            if (j < n && j - i <= 200 && s[j] == ')' && depth == 0) { out += ' '; i = j + 1; continue; }
            ++i; continue;                                         // '(' sin cerrar
        }
        if (s[i] == ')') { ++i; continue; }
        out += s[i++];
    }
    return out;
}

static string clean_free_text(const string& raw, bool lower, bool unwrap_other) {
    string s = normalize_text(raw, lower);
    s = strip_html(s);
    s = strip_brackets(s, unwrap_other);
    return squeeze(s);
}

// ----------------------------------------------------------------------------
//  Title
// ----------------------------------------------------------------------------
// "Room (2015 film)" -> "Room",  "Cheap Thrills (film)" -> "Cheap Thrills"
static string strip_film_suffix(string t) {
    if (t.size() > 2 && (ends_with(t, ")'") || ends_with(t, ")\""))) t.pop_back();
    if (t.empty() || t.back() != ')') return t;
    size_t open = t.rfind('(');
    if (open == NPOS || open == 0) return t;
    string inner = lower_ascii(trim(t.substr(open + 1, t.size() - open - 2)));
    if ((inner == "film" || ends_with(inner, " film")) && word_count(inner) <= 4) {
        t = trim(t.substr(0, open));
        ++G.title_suffix;
    }
    if (t.size() > 2 && ends_with(t, " '")) t = trim(t.substr(0, t.size() - 1));
    return t;
}
static string clean_title(const string& raw, const Options& o) {
    string t = clean_free_text(raw, o.lower, false);
    return strip_film_suffix(t);
}

// ----------------------------------------------------------------------------
//  Listas de personas (Director, Cast)
// ----------------------------------------------------------------------------
static bool is_na_word(const string& lc) {
    static const std::unordered_set<string> na = {
        "unknown", "various", "n/a", "na", "none", "tba", "tbd", "uncredited", "etc", "etc.",
        "others", "other", "-", "?", "no cast", "not known", "unnamed"};
    return na.count(lc) > 0;
}
static vector<string> split_by_chars(const string& s, const string& delims) {
    vector<string> out; string cur;
    for (char c : s) {
        if (delims.find(c) != NPOS) { out.push_back(trim(cur)); cur.clear(); }
        else cur += c;
    }
    out.push_back(trim(cur));
    out.erase(std::remove_if(out.begin(), out.end(), [](const string& x) { return x.empty(); }), out.end());
    return out;
}
// Separa por " and ". Si force=false solo separa cuando ambos lados tienen >= 2 palabras
// (así "Peter and Bobby Farrelly" no se rompe).
static vector<string> split_and(const string& seg, bool force) {
    string low = lower_ascii(seg);
    vector<string> parts; size_t start = 0;
    while (true) {
        size_t p = low.find(" and ", start);
        if (p == NPOS) { parts.push_back(seg.substr(start)); break; }
        parts.push_back(seg.substr(start, p - start));
        start = p + 5;
    }
    if (parts.size() == 1) return parts;
    if (!force) for (auto& p : parts) if (word_count(trim(p)) < 2) return {seg};
    return parts;
}
static string clean_item(string t) {
    t = trim_edges(t, " :;,*-_=+~");
    if (std::count(t.begin(), t.end(), '"') % 2 == 1) {
        if (!t.empty() && t.front() == '"') t.erase(0, 1);
        else if (!t.empty() && t.back() == '"') t.pop_back();
    }
    t = trim_edges(t, " :;,*-_=+~");
    string low = lower_ascii(t);
    if (starts_with(low, "and ")) t = trim(t.substr(4));
    return t;
}
static bool is_name_suffix(string t) {
    t = lower_ascii(t);
    while (!t.empty() && t.back() == '.') t.pop_back();
    return t == "jr" || t == "sr" || t == "ii" || t == "iii" || t == "iv";
}
static string join(const vector<string>& v, const string& sep) {
    string r;
    for (size_t i = 0; i < v.size(); ++i) { if (i) r += sep; r += v[i]; }
    return r;
}

static string clean_people(const string& raw_in, bool is_cast, const Options& o) {
    // En el original muchos repartos vienen separados por saltos de línea (\r\n)
    string raw = replace_all(replace_all(replace_all(raw_in, "\r\n", ", "), "\n", ", "), "\r", ", ");
    string s = normalize_text(raw, o.lower);
    string low = lower_ascii(s);
    // En el original, ~580 filas traen "Director: X Cast: Y" dentro de estas columnas
    if (is_cast) {
        size_t p = low.rfind("cast:");
        if (p != NPOS) { s = s.substr(p + 5); ++G.prefix_fixed; }
        else if (starts_with(trim(low), "director")) { ++G.prefix_fixed; return ""; }
        else if (starts_with(trim(low), "starring:")) { s = trim(s).substr(9); ++G.prefix_fixed; }
    } else {
        size_t p = low.find("cast:");
        if (p != NPOS) s = s.substr(0, p);
        static const char* pre[] = {"directors:", "director:", "directed by:", "directed by", "dir.:"};
        string tl = lower_ascii(trim(s));
        for (const char* pr : pre) {
            string ps(pr);
            if (starts_with(tl, ps)) { s = trim(s).substr(ps.size()); ++G.prefix_fixed; break; }
        }
    }
    s = strip_html(s);
    s = strip_brackets(s, true);
    s = squeeze(remove_parens(s));
    s = replace_all(s, " & ", " and ");

    vector<string> segs = split_by_chars(s, ",;|/");
    if (is_cast) {   // texto de trama que se coló en Cast: un "nombre" de >10 palabras
        for (auto& g : segs) if (word_count(g) > 10) { ++G.cast_garbage; return ""; }
    }
    bool multi = segs.size() > 1;
    vector<string> items;
    std::unordered_set<string> seen;
    for (auto& seg : segs) {
        for (auto& part : split_and(seg, is_cast || multi)) {
            string it = clean_item(trim(part));
            string lc = lower_ascii(it);
            if (it.size() < 2 || it.size() > 60 || !has_letter(it) || is_na_word(lc) || word_count(it) > 6) continue;
            if (is_name_suffix(it)) {                      // "George Nicholls, Jr." -> "George Nicholls Jr."
                if (!items.empty()) items.back() += " " + it;
                continue;
            }
            std::string key = normalize_text(it, true);
            if (seen.insert(key).second) items.push_back(it);
        }
    }
    return join(items, ", ");
}

// ----------------------------------------------------------------------------
//  Genre
// ----------------------------------------------------------------------------
static vector<string> genre_tokens(const string& raw) {
    string s = normalize_text(raw, true);
    s = strip_html(s);
    s = strip_brackets(s, true);
    s = squeeze(remove_parens(s));
    s = replace_all(s, " & ", " and ");
    static const char* markers[] = {" based on ", " based upon ", " adapted from ", " inspired by ",
        " co-produced ", " produced by ", " produced ", " directed by ", " starring ", " featuring ",
        " by ", " with ", " about ", " set in ", " from "};
    vector<string> out;
    std::unordered_set<string> seen;
    for (auto& seg : split_by_chars(s, ",;|/")) {
        for (auto& part : split_and(seg, true)) {
            string t = trim_edges(trim(part), " .:;,*\"-_");
            t = replace_word(t, "science-fiction", "science fiction");
            t = replace_word(t, "sci-fi", "science fiction");
            t = replace_word(t, "sci fi", "science fiction");
            t = replace_word(t, "scifi", "science fiction");
            t = replace_word(t, "rom-com", "romantic comedy");
            t = replace_word(t, "rom com", "romantic comedy");
            t = replace_word(t, "romcom", "romantic comedy");
            // "drama based on the novel by X" -> "drama"
            string pad = " " + t + " ";
            size_t cut = NPOS;
            for (const char* m : markers) { size_t p = pad.find(m); if (p != NPOS && p < cut) cut = p; }
            if (cut != NPOS) t = (cut == 0) ? "" : trim(pad.substr(1, cut - 1));
            // "drama film" -> "drama" (pero "film noir" se conserva)
            while (true) {
                if (ends_with(t, " films")) t = trim(t.substr(0, t.size() - 6));
                else if (ends_with(t, " film")) t = trim(t.substr(0, t.size() - 5));
                else if (ends_with(t, " movie")) t = trim(t.substr(0, t.size() - 6));
                else break;
            }
            if (t == "film" || t == "films" || t == "movie" || t == "movies") t.clear();
            if (t.empty() || !has_letter(t) || is_na_word(t)) continue;
            size_t w = word_count(t);
            if (w > 4 || t.size() > 40 || (has_digit(t) && w > 2)) { ++G.genre_junk; continue; }
            if (seen.insert(t).second) out.push_back(t);
        }
    }
    return out;
}

// ----------------------------------------------------------------------------
//  Origin / URL / Year
// ----------------------------------------------------------------------------
static string clean_origin(const string& raw) {
    string t = trim(squeeze(normalize_text(raw, false)));
    if (t.find('_') != NPOS) { t = replace_all(t, "_", " "); ++G.origin_fixed; }
    return t;
}
static string clean_url(const string& raw) {
    string u = trim(normalize_text(raw, false));
    if (u.empty()) return "";
    if (u.find("redlink=1") != NPOS || u.find("action=edit") != NPOS ||
        !starts_with(u, "https://en.wikipedia.org/wiki/")) { ++G.bad_url; return ""; }
    return u;
}
static bool parse_year(const string& raw, int& y) {
    string t = trim(normalize_text(raw, false));
    if (t.empty() || t.size() > 4) return false;
    for (char c : t) if (c < '0' || c > '9') return false;
    y = std::atoi(t.c_str());
    return y >= MIN_YEAR && y <= MAX_YEAR;
}

// ----------------------------------------------------------------------------
//  Lectura / escritura CSV (RFC 4180, con saltos de línea dentro de comillas)
// ----------------------------------------------------------------------------
static bool parse_record(const string& b, size_t& pos, vector<string>& rec) {
    rec.clear();
    size_t n = b.size();
    if (pos >= n) return false;
    string field; bool in_q = false;
    while (pos < n) {
        char c = b[pos];
        if (in_q) {
            if (c == '"') {
                if (pos + 1 < n && b[pos + 1] == '"') { field += '"'; pos += 2; continue; }
                in_q = false; ++pos; continue;
            }
            field += c; ++pos; continue;
        }
        if (c == '"' && field.empty()) { in_q = true; ++pos; continue; }
        if (c == ',') { rec.push_back(std::move(field)); field.clear(); ++pos; continue; }
        if (c == '\r' || c == '\n') {
            if (c == '\r' && pos + 1 < n && b[pos + 1] == '\n') ++pos;
            ++pos; rec.push_back(std::move(field)); return true;
        }
        field += c; ++pos;
    }
    rec.push_back(std::move(field));
    return true;
}
static void put_field(string& out, const string& f) {
    if (f.find_first_of(",\"\n\r") == NPOS) { out += f; return; }
    out += '"';
    for (char c : f) { if (c == '"') out += '"'; out += c; }
    out += '"';
}

// ----------------------------------------------------------------------------
//  Fila limpia + deduplicación
// ----------------------------------------------------------------------------
struct Row {
    size_t src = 0; int year = 0;
    string title, origin, director, cast, genre, wiki, plot, person_lc;
    vector<string> gtok;
};
struct Reject { size_t src; string reason, title, year; };

static int completeness(const Row& r) {
    return (int)!r.director.empty() + (int)!r.cast.empty() + (int)!r.gtok.empty() + (int)!r.wiki.empty();
}
static void fill_empty(Row& keep, const Row& other) {
    if (keep.director.empty()) keep.director = other.director;
    if (keep.cast.empty()) keep.cast = other.cast;
    if (keep.gtok.empty()) keep.gtok = other.gtok;
    if (keep.wiki.empty()) keep.wiki = other.wiki;
    keep.person_lc = normalize_text(keep.cast + " , " + keep.director, true);
}

static int find_col(const vector<string>& header, const char* name) {
    string want = lower_ascii(name);
    for (size_t i = 0; i < header.size(); ++i)
        if (lower_ascii(trim(normalize_text(header[i], false))) == want) return (int)i;
    return -1;
}

int main(int argc, char** argv) {
    Options opt;
    vector<string> pos_args;
    for (int i = 1; i < argc; ++i) {
        string a = argv[i];
        if (a == "--keep-case") opt.lower = false;
        else if (a == "--bom") opt.bom = true;
        else if (a == "--min-plot" && i + 1 < argc) opt.min_plot = (size_t)std::atoi(argv[++i]);
        else if (a == "--help" || a == "-h") {
            std::cout << "Uso: clean_movies [entrada.csv] [salida.csv] [--keep-case] [--min-plot N] [--bom]\n";
            return 0;
        } else pos_args.push_back(a);
    }
    if (pos_args.size() > 0) opt.in = pos_args[0];
    if (pos_args.size() > 1) opt.out = pos_args[1];

    // --- leer archivo completo ---
    std::ifstream fin(opt.in, std::ios::binary);
    if (!fin) { std::cerr << "No se pudo abrir: " << opt.in << "\n"; return 1; }
    string buf((std::istreambuf_iterator<char>(fin)), std::istreambuf_iterator<char>());
    fin.close();
    size_t pos = 0;
    if (starts_with(buf, "\xEF\xBB\xBF")) pos = 3;

    vector<string> rec;
    if (!parse_record(buf, pos, rec)) { std::cerr << "Archivo vacío.\n"; return 1; }
    const vector<string> header = rec;
    int cY = find_col(header, "Release Year"), cT = find_col(header, "Title"),
        cO = find_col(header, "Origin/Ethnicity"), cD = find_col(header, "Director"),
        cC = find_col(header, "Cast"), cG = find_col(header, "Genre"),
        cW = find_col(header, "Wiki Page"), cP = find_col(header, "Plot");
    if (cY < 0 || cT < 0 || cO < 0 || cD < 0 || cC < 0 || cG < 0 || cW < 0 || cP < 0) {
        std::cerr << "Faltan columnas esperadas en el encabezado.\n"; return 1;
    }

    vector<Row> rows;
    vector<Reject> rejects;
    std::unordered_map<string, size_t> map_wiki, map_plot;

    // --- Pasada 1: limpiar fila por fila + deduplicar ---
    while (parse_record(buf, pos, rec)) {
        if (rec.size() == 1 && rec[0].empty()) continue;           // línea en blanco
        ++G.read;
        size_t src = G.read + 1;                                    // nº de registro (1 = encabezado)
        auto reject = [&](const string& why, const string& t, const string& y) {
            rejects.push_back({src, why, t, y});
        };
        if (rec.size() != header.size()) { ++G.rej_cols; reject("columnas distintas al encabezado", "", ""); continue; }

        Row r; r.src = src;
        if (!parse_year(rec[cY], r.year)) { ++G.rej_year; reject("año inválido", rec[cT], rec[cY]); continue; }
        r.title = clean_title(rec[cT], opt);
        if (r.title.empty()) { ++G.rej_title; reject("título vacío", rec[cT], rec[cY]); continue; }
        r.plot = clean_free_text(rec[cP], opt.lower, false);
        string plot_lc = lower_ascii(r.plot);
        if (r.plot.empty()) { ++G.rej_plot_empty; reject("trama vacía", r.title, rec[cY]); continue; }
        if (r.plot.size() < opt.min_plot) { ++G.rej_plot_short; reject("trama demasiado corta", r.title, rec[cY]); continue; }
        if ((r.plot.size() < 80 && r.plot.back() == ':') ||
            (r.plot.size() < 60 && starts_with(plot_lc, "see "))) {
            ++G.rej_plot_trunc; reject("trama truncada o sin contenido", r.title, rec[cY]); continue;
        }
        r.origin = clean_origin(rec[cO]);
        r.wiki = clean_url(rec[cW]);
        r.director = clean_people(rec[cD], false, opt);
        r.cast = clean_people(rec[cC], true, opt);
        r.gtok = genre_tokens(rec[cG]);
        r.person_lc = normalize_text(r.cast + " , " + r.director, true);

        // duplicados: misma página de Wikipedia + año, o mismo título + año + trama
        string k1 = r.wiki.empty() ? "" : lower_ascii(r.wiki) + "|" + std::to_string(r.year);
        string k2 = normalize_text(r.title, true) + "|" + std::to_string(r.year) + "|" +
                    std::to_string(std::hash<string>{}(r.plot));
        size_t idx = NPOS;
        if (!k1.empty()) { auto it = map_wiki.find(k1); if (it != map_wiki.end()) idx = it->second; }
        if (idx == NPOS) { auto it = map_plot.find(k2); if (it != map_plot.end()) idx = it->second; }
        if (idx != NPOS) {
            ++G.rej_dup;
            Row& keep = rows[idx];
            size_t first_src = keep.src;
            if (completeness(r) > completeness(keep)) { std::swap(keep, r); keep.src = first_src; }
            fill_empty(keep, r);
            reject("duplicado de la fila " + std::to_string(first_src), r.title, std::to_string(r.year));
            if (!k1.empty()) map_wiki.emplace(k1, idx);
            map_plot.emplace(k2, idx);
            continue;
        }
        if (!k1.empty()) map_wiki.emplace(k1, rows.size());
        map_plot.emplace(k2, rows.size());
        rows.push_back(std::move(r));
    }

    // --- Pasada 2: géneros. Se descarta como "género" lo que en realidad es un
    //     nombre de actor/director de la misma fila (error típico del original) ---
    std::unordered_map<string, int> freq;
    std::unordered_set<string> people;                       // todos los nombres de Cast/Director
    for (auto& r : rows) {
        for (auto& t : r.gtok) ++freq[t];
        for (auto& nm : split_by_chars(r.person_lc, ",")) people.insert(nm);
    }
    for (auto& r : rows) {
        vector<string> keep;
        for (auto& t : r.gtok) {
            if (freq[t] < 20 && t.size() >= 3 &&
                (contains_word_seq(r.person_lc, t) || people.count(t))) { ++G.genre_leak; continue; }
            keep.push_back(t);
        }
        r.genre = join(keep, ", ");
    }

    // --- escribir salida ---
    std::ofstream fout(opt.out, std::ios::binary);
    if (!fout) { std::cerr << "No se pudo crear: " << opt.out << "\n"; return 1; }
    if (opt.bom) fout << "\xEF\xBB\xBF";
    fout << "Release Year,Title,Origin/Ethnicity,Director,Cast,Genre,Wiki Page,Plot\n";
    string line;
    for (auto& r : rows) {
        line.clear();
        line += std::to_string(r.year); line += ',';
        put_field(line, r.title);    line += ',';
        put_field(line, r.origin);   line += ',';
        put_field(line, r.director); line += ',';
        put_field(line, r.cast);     line += ',';
        put_field(line, r.genre);    line += ',';
        put_field(line, r.wiki);     line += ',';
        put_field(line, r.plot);     line += '\n';
        fout.write(line.data(), (std::streamsize)line.size());
        ++G.written;
    }
    fout.close();

    // --- archivo de descartadas ---
    string stem = opt.out;
    size_t dot = stem.rfind('.');
    if (dot != NPOS && dot > 0) stem = stem.substr(0, dot);
    string rej_path = stem + "_descartadas.csv";
    std::ofstream frej(rej_path, std::ios::binary);
    frej << "registro_original,motivo,titulo,anio\n";
    for (auto& x : rejects) {
        line.clear();
        line += std::to_string(x.src); line += ',';
        put_field(line, x.reason); line += ',';
        put_field(line, x.title); line += ',';
        put_field(line, x.year); line += '\n';
        frej.write(line.data(), (std::streamsize)line.size());
    }
    frej.close();

    // --- resumen ---
    std::cout << "Filas leídas:               " << G.read << "\n"
              << "Filas escritas:             " << G.written << "  -> " << opt.out << "\n"
              << "Filas descartadas:          " << rejects.size() << "  -> " << rej_path << "\n"
              << "   columnas incorrectas:    " << G.rej_cols << "\n"
              << "   año inválido:            " << G.rej_year << "\n"
              << "   título vacío:            " << G.rej_title << "\n"
              << "   trama vacía/corta/trunca:" << (G.rej_plot_empty + G.rej_plot_short + G.rej_plot_trunc) << "\n"
              << "   duplicadas:              " << G.rej_dup << "\n"
              << "Correcciones:\n"
              << "   notas [n]/[citation needed]/<ref> quitadas: " << G.refs_removed << "\n"
              << "   prefijos 'Director:'/'Cast:' corregidos:     " << G.prefix_fixed << "\n"
              << "   Cast con texto de trama vaciado:             " << G.cast_garbage << "\n"
              << "   sufijos '(film)' quitados de Title:          " << G.title_suffix << "\n"
              << "   géneros basura descartados:                  " << G.genre_junk << "\n"
              << "   'géneros' que eran nombres de reparto:       " << G.genre_leak << "\n"
              << "   Origin/Ethnicity con '_' corregido:          " << G.origin_fixed << "\n"
              << "   Wiki Page inválida (redlink/edición) vaciada:" << G.bad_url << "\n";
    return 0;
}
