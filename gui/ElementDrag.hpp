#pragma once

// Drag & drop payload of a model element (Model Explorer → diagram canvas)

#include <QMimeData>
#include <optional>

namespace ea::gui {

inline constexpr const char* ElementMimeType = "application/x-onlyarchitect-element";

inline QMimeData* encodeElementDrag(int elementId) {
    auto* mime = new QMimeData();
    mime->setData(ElementMimeType, QByteArray::number(elementId));
    return mime;
}

inline std::optional<int> decodeElementDrag(const QMimeData* mime) {
    if (!mime || !mime->hasFormat(ElementMimeType)) return std::nullopt;
    bool ok = false;
    const int id = mime->data(ElementMimeType).toInt(&ok);
    return ok && id > 0 ? std::optional<int>(id) : std::nullopt;
}

} // namespace ea::gui
