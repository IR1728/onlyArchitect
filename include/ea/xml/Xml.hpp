#pragma once

// Minimal XML 1.0 reader (DOM) and writer for model interchange (XMI). No external dependency.
// Supports elements, attributes, character data, CDATA, comments, processing instructions and a
// DOCTYPE (skipped), the predefined entities and numeric character references. No DTD expansion.

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ea::xml {

struct Node {
    ~Node();

    std::string name; // qualified, e.g. "uml:Model"
    std::vector<std::pair<std::string, std::string>> attributes;
    std::vector<std::unique_ptr<Node>> children; // element children, in document order
    std::string text;                            // character data directly inside this element
    Node* parent{nullptr};

    // Attribute by qualified name ("xmi:id") or, if none matches, by local name ("k" finds "ns:k");
    // the local-name fallback never matches xmi:* attributes ("type" is not "xmi:type")
    const std::string* attribute(std::string_view qualifiedOrLocal) const;
    std::string attr(std::string_view qualifiedOrLocal, std::string fallback = "") const;
    std::string_view localName() const;
    // First child whose qualified or local name matches
    const Node* child(std::string_view name) const;
    std::vector<const Node*> childrenNamed(std::string_view name) const;
};

struct ParseResult {
    std::unique_ptr<Node> root;
    std::string error; // empty on success
    size_t line{0};    // where the error was found
    bool ok() const { return root != nullptr && error.empty(); }
};

ParseResult parse(std::string_view document);

// Escapes for attribute values and text (newlines and tabs kept as character references)
std::string escape(std::string_view text);

// Streaming writer with indentation; attributes are written only while a start tag is open
class Writer {
public:
    Writer();
    Writer& open(std::string_view name);
    Writer& attr(std::string_view name, std::string_view value);
    Writer& attr(std::string_view name, int value);
    Writer& attrIfNotEmpty(std::string_view name, std::string_view value);
    Writer& text(std::string_view text);
    Writer& close();
    const std::string& str();

private:
    void finishStartTag(bool hasContent);
    std::string m_out;
    std::vector<std::string> m_stack;
    bool m_tagOpen{false};
    bool m_lastWasText{false};
};

} // namespace ea::xml
