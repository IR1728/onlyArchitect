#include "ea/editor/ElementSnapshot.hpp"

namespace ea::editor {

ElementSnapshot ElementSnapshot::capture(const Element& elem) {
    ElementSnapshot s;
    s.name = elem.GetName();
    s.alias = elem.GetAlias();
    s.keywords = elem.GetKeywords();
    s.subtype = elem.GetSubtype();
    s.stereotype = elem.GetStereotype();
    s.notes = elem.GetNotes();
    s.status = elem.GetStatus();
    s.author = elem.GetAuthor();
    s.version = elem.GetVersion();
    s.type = elem.GetType();
    s.isAbstract = elem.IsAbstract();
    s.attributes.reserve(elem.GetAttributes().size());
    for (const auto& attr : elem.GetAttributes()) {
        if (attr) s.attributes.push_back(*attr);
    }
    s.methods.reserve(elem.GetMethods().size());
    for (const auto& meth : elem.GetMethods()) {
        if (meth) s.methods.push_back(*meth);
    }
    return s;
}

void ElementSnapshot::restoreInto(Element& elem) const {
    elem.SetName(name);
    elem.SetAlias(alias);
    elem.SetKeywords(keywords);
    elem.SetSubtype(subtype);
    elem.SetStereotype(stereotype);
    elem.SetNotes(notes);
    elem.SetStatus(status);
    elem.SetAuthor(author);
    elem.SetVersion(version);
    elem.SetType(type);
    elem.SetAbstract(isAbstract);
    // Fresh copies: the snapshot must stay intact for the next undo/redo
    elem.GetAttributes().Clear();
    for (const auto& attr : attributes) {
        elem.GetAttributes().AddExisting(std::make_shared<Attribute>(attr));
    }
    elem.GetMethods().Clear();
    for (const auto& meth : methods) {
        elem.GetMethods().AddExisting(std::make_shared<Method>(meth));
    }
}

} // namespace ea::editor
