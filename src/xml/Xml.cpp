#include "ea/xml/Xml.hpp"
#include <cctype>
#include <cstdint>
#include <stdexcept>

namespace ea::xml {

// ============================================================================
// Node
// ============================================================================

Node::~Node() {
    // Iterative destruction so deeply nested documents (e.g. 100k levels) cannot
    // overflow small default thread stacks (such as 1-2 MiB on Windows).
    if (children.empty()) return;
    std::vector<std::unique_ptr<Node>> stack;
    for (auto& c : children) {
        if (c) stack.push_back(std::move(c));
    }
    children.clear();
    while (!stack.empty()) {
        auto cur = std::move(stack.back());
        stack.pop_back();
        for (auto& c : cur->children) {
            if (c) stack.push_back(std::move(c));
        }
        cur->children.clear();
    }
}

namespace {

std::string_view local_of(std::string_view qualified) {
    const auto colon = qualified.find(':');
    return colon == std::string_view::npos ? qualified : qualified.substr(colon + 1);
}

bool name_matches(std::string_view qualified, std::string_view wanted) {
    return qualified == wanted || (wanted.find(':') == std::string_view::npos && local_of(qualified) == wanted);
}

} // namespace

const std::string* Node::attribute(std::string_view name) const {
    for (const auto& [k, v] : attributes) if (k == name) return &v;
    if (name.find(':') == std::string_view::npos) {
        // never let "type" find "xmi:type": the xmi namespace has its own meaning
        for (const auto& [k, v] : attributes) if (local_of(k) == name && k.rfind("xmi:", 0) != 0) return &v;
    }
    return nullptr;
}

std::string Node::attr(std::string_view name, std::string fallback) const {
    const auto* v = attribute(name);
    return v ? *v : std::move(fallback);
}

std::string_view Node::localName() const {
    return local_of(name);
}

const Node* Node::child(std::string_view wanted) const {
    for (const auto& c : children) if (name_matches(c->name, wanted)) return c.get();
    return nullptr;
}

std::vector<const Node*> Node::childrenNamed(std::string_view wanted) const {
    std::vector<const Node*> out;
    for (const auto& c : children) if (name_matches(c->name, wanted)) out.push_back(c.get());
    return out;
}

// ============================================================================
// Parser (iterative: deep documents cannot overflow the stack)
// ============================================================================

namespace {

struct ParseError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

class Parser {
public:
    static constexpr size_t kMaxDocumentBytes = 64 * 1024 * 1024; // 64 MiB DoS cap
    static constexpr size_t kMaxNodes = 1000000;
    static constexpr size_t kMaxAttributesPerNode = 512;

    explicit Parser(std::string_view s) : m_s(s) {}

    std::unique_ptr<Node> run() {
        if (m_s.size() > kMaxDocumentBytes) fail("Documento XML demasiado grande (> 64 MiB)");
        if (m_s.find('\0') != std::string_view::npos) fail("Contenido binario o bytes NUL detectados: no es un documento XML válido");
        if (m_s.substr(0, 3) == "\xEF\xBB\xBF") m_pos = 3; // UTF-8 BOM
        skipSpace();
        if (m_pos >= m_s.size() || peek() != '<') fail("El documento no comienza con una etiqueta XML ('<')");
        std::unique_ptr<Node> root;
        std::vector<Node*> stack;
        size_t nodeCount = 0;
        while (m_pos < m_s.size()) {
            if (peek() != '<') {
                const size_t start = m_pos;
                while (m_pos < m_s.size() && peek() != '<') advance();
                const auto raw = m_s.substr(start, m_pos - start);
                if (!stack.empty()) stack.back()->text += decode(raw);
                else if (!is_blank(raw)) fail("Texto fuera del elemento raíz");
                continue;
            }
            if (starts("<?")) { skipPast("?>"); continue; }
            if (starts("<!--")) { skipPast("-->"); continue; }
            if (starts("<![CDATA[")) {
                m_pos += 9;
                const size_t end = m_s.find("]]>", m_pos);
                if (end == std::string_view::npos) fail("CDATA sin cerrar");
                if (stack.empty()) fail("CDATA fuera del elemento raíz");
                stack.back()->text += std::string(m_s.substr(m_pos, end - m_pos));
                advanceTo(end + 3);
                continue;
            }
            if (starts("<!")) { skipDoctype(); continue; }
            if (starts("</")) {
                m_pos += 2;
                const auto name = readName();
                skipSpace();
                expect('>');
                if (stack.empty() || stack.back()->name != name) fail("Etiqueta de cierre inesperada: </" + std::string(name) + ">");
                stack.pop_back();
                continue;
            }
            // start tag
            advance(); // '<'
            if (++nodeCount > kMaxNodes) fail("Límite máximo de nodos XML excedido (prevención DoS)");
            auto node = std::make_unique<Node>();
            node->name = std::string(readName());
            bool selfClosing = false;
            for (;;) {
                skipSpace();
                if (starts("/>")) { m_pos += 2; selfClosing = true; break; }
                if (peek() == '>') { advance(); break; }
                if (node->attributes.size() >= kMaxAttributesPerNode) fail("Demasiados atributos en un solo elemento XML");
                auto key = std::string(readName());
                skipSpace();
                expect('=');
                skipSpace();
                const char quote = peek();
                if (quote != '"' && quote != '\'') fail("Valor de atributo sin comillas");
                advance();
                const size_t start = m_pos;
                while (m_pos < m_s.size() && peek() != quote) advance();
                if (m_pos >= m_s.size()) fail("Valor de atributo sin cerrar");
                node->attributes.emplace_back(std::move(key), decode(m_s.substr(start, m_pos - start)));
                advance();
            }
            Node* raw = node.get();
            if (stack.empty()) {
                if (root) fail("Más de un elemento raíz");
                root = std::move(node);
            } else {
                raw->parent = stack.back();
                stack.back()->children.push_back(std::move(node));
            }
            if (!selfClosing) stack.push_back(raw);
        }
        if (!stack.empty()) fail("Falta el cierre de <" + stack.back()->name + ">");
        if (!root) fail("Documento vacío");
        return root;
    }

    size_t line() const { return m_line; }

private:
    char peek() const { return m_pos < m_s.size() ? m_s[m_pos] : '\0'; }
    void advance() {
        if (m_s[m_pos] == '\n') ++m_line;
        ++m_pos;
    }
    void advanceTo(size_t pos) { while (m_pos < pos) advance(); }
    bool starts(std::string_view p) const { return m_s.substr(m_pos, p.size()) == p; }
    [[noreturn]] void fail(const std::string& what) const { throw ParseError(what); }
    void expect(char c) {
        if (peek() != c) fail(std::string("Se esperaba '") + c + "'");
        advance();
    }
    void skipSpace() { while (m_pos < m_s.size() && std::isspace(static_cast<unsigned char>(peek()))) advance(); }
    void skipPast(std::string_view end) {
        const size_t at = m_s.find(end, m_pos);
        if (at == std::string_view::npos) fail("Falta " + std::string(end));
        advanceTo(at + end.size());
    }
    void skipDoctype() {
        const size_t start = m_pos;
        int depth = 0; // internal subset in [...]
        while (m_pos < m_s.size()) {
            const char c = peek();
            advance();
            if (c == '[') ++depth;
            else if (c == ']') --depth;
            else if (c == '>' && depth <= 0) {
                const std::string_view block = m_s.substr(start, m_pos - start);
                // Block XXE / Billion Laughs / external DTD fetches explicitly
                if (block.find("<!ENTITY") != std::string_view::npos ||
                    block.find("<!entity") != std::string_view::npos) {
                    fail("Declaraciones <!ENTITY> bloqueadas por seguridad (prevención XXE / XML Bomb)");
                }
                if (block.find(" SYSTEM ") != std::string_view::npos ||
                    block.find(" PUBLIC ") != std::string_view::npos ||
                    block.find("\tSYSTEM") != std::string_view::npos ||
                    block.find("\nSYSTEM") != std::string_view::npos) {
                    fail("Referencias DTD externas (SYSTEM/PUBLIC) bloqueadas por seguridad (prevención XXE)");
                }
                return;
            }
        }
        fail("DOCTYPE sin cerrar");
    }
    std::string_view readName() {
        const size_t start = m_pos;
        while (m_pos < m_s.size()) {
            const char c = peek();
            if (std::isspace(static_cast<unsigned char>(c)) || c == '>' || c == '/' || c == '=' || c == '<') break;
            advance();
        }
        if (m_pos == start) fail("Nombre vacío");
        return m_s.substr(start, m_pos - start);
    }
    static bool is_blank(std::string_view s) {
        for (char c : s) if (!std::isspace(static_cast<unsigned char>(c))) return false;
        return true;
    }
    static void append_utf8(std::string& out, uint32_t cp) {
        if (cp < 0x80) out += static_cast<char>(cp);
        else if (cp < 0x800) { out += static_cast<char>(0xC0 | (cp >> 6)); out += static_cast<char>(0x80 | (cp & 0x3F)); }
        else if (cp < 0x10000) {
            out += static_cast<char>(0xE0 | (cp >> 12));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (cp >> 18));
            out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }
    std::string decode(std::string_view raw) const {
        std::string out;
        out.reserve(raw.size());
        for (size_t i = 0; i < raw.size(); ++i) {
            if (raw[i] != '&') { out += raw[i]; continue; }
            const size_t semi = raw.find(';', i);
            if (semi == std::string_view::npos) fail("Entidad sin ';'");
            const auto ent = raw.substr(i + 1, semi - i - 1);
            if (ent == "lt") out += '<';
            else if (ent == "gt") out += '>';
            else if (ent == "amp") out += '&';
            else if (ent == "quot") out += '"';
            else if (ent == "apos") out += '\'';
            else if (ent.size() > 1 && ent[0] == '#') {
                uint32_t cp = 0;
                try {
                    cp = ent[1] == 'x' || ent[1] == 'X' ? static_cast<uint32_t>(std::stoul(std::string(ent.substr(2)), nullptr, 16))
                                                        : static_cast<uint32_t>(std::stoul(std::string(ent.substr(1))));
                } catch (const std::exception&) {
                    fail("Referencia de carácter no válida: &" + std::string(ent) + ";");
                }
                if (cp == 0 || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF) ||
                    (cp < 0x20 && cp != 0x09 && cp != 0x0A && cp != 0x0D)) {
                    fail("Referencia de carácter XML ilegal o fuera de rango");
                }
                append_utf8(out, cp);
            } else {
                fail("Entidad desconocida: &" + std::string(ent) + ";");
            }
            i = semi;
        }
        return out;
    }

    std::string_view m_s;
    size_t m_pos{0};
    size_t m_line{1};
};

} // namespace

ParseResult parse(std::string_view document) {
    Parser p(document);
    ParseResult result;
    try {
        result.root = p.run();
    } catch (const ParseError& e) {
        result.root.reset();
        result.error = e.what();
        result.line = p.line();
    }
    return result;
}

// ============================================================================
// Writer
// ============================================================================

std::string escape(std::string_view text) {
    std::string out;
    out.reserve(text.size() + 8);
    for (char c : text) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\n': out += "&#10;"; break;
            case '\r': out += "&#13;"; break;
            case '\t': out += "&#9;"; break;
            default:
                // control characters are not allowed in XML 1.0: drop them
                if (static_cast<unsigned char>(c) >= 0x20 || c < 0) out += c;
        }
    }
    return out;
}

Writer::Writer() {
    m_out = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
}

void Writer::finishStartTag(bool hasContent) {
    if (!m_tagOpen) return;
    m_out += hasContent ? ">" : "";
    m_tagOpen = false;
}

Writer& Writer::open(std::string_view name) {
    finishStartTag(true);
    if (!m_stack.empty() && !m_lastWasText) m_out += '\n';
    m_out.append(m_stack.size() * 2, ' ');
    m_out += '<';
    m_out += name;
    m_stack.emplace_back(name);
    m_tagOpen = true;
    m_lastWasText = false;
    return *this;
}

Writer& Writer::attr(std::string_view name, std::string_view value) {
    if (!m_tagOpen) throw std::logic_error("xml::Writer::attr outside a start tag");
    m_out += ' ';
    m_out += name;
    m_out += "=\"";
    m_out += escape(value);
    m_out += '"';
    return *this;
}

Writer& Writer::attr(std::string_view name, int value) {
    return attr(name, std::to_string(value));
}

Writer& Writer::attrIfNotEmpty(std::string_view name, std::string_view value) {
    return value.empty() ? *this : attr(name, value);
}

Writer& Writer::text(std::string_view t) {
    finishStartTag(true);
    m_out += escape(t);
    m_lastWasText = true;
    return *this;
}

Writer& Writer::close() {
    if (m_stack.empty()) throw std::logic_error("xml::Writer::close without open element");
    if (m_tagOpen) {
        m_out += "/>";
        m_tagOpen = false;
    } else {
        if (!m_lastWasText) {
            m_out += '\n';
            m_out.append((m_stack.size() - 1) * 2, ' ');
        }
        m_out += "</" + m_stack.back() + ">";
    }
    m_stack.pop_back();
    m_lastWasText = false;
    if (m_stack.empty()) m_out += '\n';
    return *this;
}

const std::string& Writer::str() {
    while (!m_stack.empty()) close();
    return m_out;
}

} // namespace ea::xml
