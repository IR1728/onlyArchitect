#include "UmlSymbolPainter.hpp"
#include "DarkTheme.hpp"
#include "ea/editor/FeatureText.hpp"

#include <QFontMetricsF>
#include <QLinearGradient>
#include <QPainterPath>
#include <QRegularExpression>

namespace uml_symbols {

namespace {

using ea::ObjectType;
using ea::gui::M3Spacing;
using ea::gui::M3Typography;
using ea::gui::m3;
using ea::gui::withAlpha;

constexpr qreal CornerRadius = M3Spacing::ShapeMedium;
constexpr qreal PadX = M3Spacing::UmlCompartmentPadX;
constexpr qreal PadY = M3Spacing::UmlCompartmentPadY;
constexpr qreal Gutter = M3Spacing::UmlVisibilityGutter;
constexpr qreal LineH = M3Spacing::UmlFeatureLineH;
constexpr qreal BorderWidth = 1.0;
constexpr qreal StereotypeLineH = 14.0;
constexpr qreal NameLineH = 20.0;
constexpr qreal ShadowOffsetY = 3.0;
constexpr int ShadowAlpha = 110;
constexpr qreal NoteFold = 12.0;

QColor mix(const QColor& a, const QColor& b, qreal t) {
    return QColor::fromRgbF(a.redF() * (1 - t) + b.redF() * t, a.greenF() * (1 - t) + b.greenF() * t,
                            a.blueF() * (1 - t) + b.blueF() * t);
}

// Tonal identity of each symbol kind: body fill and accent (border / glyphs)
struct Tones {
    QColor body;
    QColor header;
    QColor accent;
};

Tones tonesFor(ObjectType type) {
    const auto& c = ea::gui::m3();
    switch (type) {
        case ObjectType::Interface: return {c.surfaceContainer, c.surfaceHigh, c.tertiary};
        case ObjectType::Enumeration: return {c.surfaceContainer, c.surfaceHigh, c.secondary};
        case ObjectType::Package: return {c.surface, c.surfaceContainer, c.secondary};
        case ObjectType::Note: return {mix(c.surfaceContainer, c.warning, 0.06), c.surfaceHigh, c.warning};
        case ObjectType::UseCase: return {c.surfaceContainer, c.surfaceHigh, c.primary};
        case ObjectType::Actor: return {c.surfaceContainer, c.surfaceHigh, c.success};
        case ObjectType::Component: return {c.primaryContainer, c.surfaceHigh, c.primary};
        case ObjectType::Port: return {c.surfaceHigh, c.surfaceHigh, c.secondary};
        default: return {c.surfaceContainer, c.surfaceHigh, c.outline};
    }
}

QColor visibilityColor(ea::Scope scope) {
    const auto& c = ea::gui::m3();
    switch (scope) {
        case ea::Scope::Private: return c.error;
        case ea::Scope::Protected: return c.warning;
        case ea::Scope::Package: return c.secondary;
        case ea::Scope::Public: break;
    }
    return c.success;
}

QPainterPath outlinePath(const QRectF& rect, ObjectType type) {
    QPainterPath path;
    if (type == ObjectType::UseCase) {
        path.addEllipse(rect);
    } else if (type == ObjectType::Port) {
        path.addRoundedRect(rect, M3Spacing::ShapeExtraSmall, M3Spacing::ShapeExtraSmall);
    } else {
        path.addRoundedRect(rect, CornerRadius, CornerRadius);
    }
    return path;
}

// M3 elevation 1: soft ambient shadow instead of a hard drop shadow
void paintShadow(QPainter* p, const QPainterPath& path) {
    p->fillPath(path.translated(0, ShadowOffsetY), withAlpha(ea::gui::m3().surfaceLowest, ShadowAlpha));
}

void paintCard(QPainter* p, const QPainterPath& path, const QColor& body, const QColor& border) {
    paintShadow(p, path);
    p->fillPath(path, body);
    p->setPen(QPen(border, BorderWidth));
    p->setBrush(Qt::NoBrush);
    p->drawPath(path);
}

void drawCentered(QPainter* p, const QRectF& r, const QString& text, const QFont& font, const QColor& color,
                  int flags = Qt::AlignCenter) {
    p->setFont(font);
    p->setPen(color);
    p->drawText(r, flags, QFontMetricsF(font).elidedText(text, Qt::ElideRight, r.width()));
}

QString guillemets(const QString& stereotype) { return QStringLiteral("«%1»").arg(stereotype); }
QString decodeEaEntities(QString text);

// ---------------------------------------------------------------------------
// Classifiers (class, interface, enumeration, package, ...)
// ---------------------------------------------------------------------------
qreal headerHeight(const SymbolContent& c, const QRectF& rect) {
    const bool hasFeatures = !c.attributes.empty() || !c.operations.empty();
    if (hasFeatures && rect.height() <= 92.0) {
        return c.stereotype.isEmpty() ? 22.0 : 32.0;
    }
    return c.stereotype.isEmpty() ? M3Spacing::UmlHeaderHeight : M3Spacing::UmlHeaderHeightStereotyped;
}

void paintHeader(QPainter* p, const QRectF& rect, const QPainterPath& card, const SymbolContent& c, const Tones& t) {
    const bool hasBody = !c.attributes.empty() || !c.operations.empty();
    const qreal hh = headerHeight(c, rect);
    const QRectF header(rect.left(), rect.top(), rect.width(), (!hasBody && rect.height() <= 85.0) ? rect.height() : hh);
    p->save();
    p->setClipPath(card); // the gradient follows the 12 px corners exactly
    QLinearGradient gradient(header.topLeft(), header.bottomRight());
    gradient.setColorAt(0.0, t.header);
    gradient.setColorAt(1.0, mix(t.header, t.body, 0.5));
    p->fillRect(header, gradient);
    p->restore();

    if (hasBody || rect.height() > 85.0) {
        p->setPen(QPen(ea::gui::m3().outlineVariant, BorderWidth));
        p->drawLine(QPointF(rect.left(), rect.top() + hh), QPointF(rect.right(), rect.top() + hh));
    }

    const qreal availW = std::max(24.0, rect.width() - 8.0);
    qreal y = rect.top() + (c.stereotype.isEmpty() ? 2.0 : 3.0);
    if (!c.stereotype.isEmpty()) {
        drawCentered(p, QRectF(rect.left() + 4.0, y, availW, StereotypeLineH), guillemets(c.stereotype),
                     M3Typography::labelMedium(), t.accent == ea::gui::m3().outline ? ea::gui::m3().primary : t.accent);
        y += StereotypeLineH;
    }

    const QString cleanName = decodeEaEntities(c.name);
    QFont nameFont = (rect.width() < 120.0 || rect.height() <= 85.0) ? M3Typography::labelMedium() : M3Typography::titleMedium();
    nameFont.setBold(true);
    // Shrink smoothly in pixels (M3Typography uses pixelSize) so long CamelCase names fit without clipping
    int px = nameFont.pixelSize() > 0 ? nameFont.pixelSize() : 11;
    const QStringList words = cleanName.split(' ', Qt::SkipEmptyParts);
    auto longestWordWidth = [&]() {
        const QFontMetricsF fm(nameFont);
        qreal maxW = 0.0;
        for (const QString& w : words) maxW = std::max(maxW, fm.horizontalAdvance(w));
        return maxW;
    };
    while (px > 7 && longestWordWidth() > availW) {
        --px;
        nameFont.setPixelSize(px);
    }

    p->setFont(nameFont);
    p->setPen(ea::gui::m3().onSurface);
    const qreal nameBottom = (!hasBody && rect.height() <= 85.0) ? (rect.bottom() - 3.0) : (rect.top() + hh - 2.0);
    const QRectF nameRect(rect.left() + 4.0, y, availW, std::max(14.0, nameBottom - y));
    p->drawText(nameRect, Qt::AlignCenter | Qt::TextWordWrap, cleanName);
}

// One compartment; adapts row height smoothly so compact EA boxes (~70 px) show all attributes
qreal paintFeatures(QPainter* p, const QRectF& rect, qreal y, const std::vector<FeatureLine>& features,
                    const QColor& signatureColor, bool compact) {
    QFont font = M3Typography::bodySmallMono();
    qreal lineH = LineH;
    qreal padY = PadY;
    qreal padX = PadX;
    qreal gutter = Gutter;
    if (compact) {
        font.setPointSizeF(7.8);
        lineH = 14.0;
        padY = 3.0;
        padX = 6.0;
        gutter = 11.0;
    }
    const QFontMetricsF fm(font);
    p->setFont(font);
    const qreal textLeft = rect.left() + padX + gutter;
    const qreal textWidth = std::max(16.0, rect.width() - 2 * padX - gutter);

    y += padY;
    for (const auto& f : features) {
        if (y + lineH > rect.bottom() - BorderWidth + 1.0) return rect.bottom();
        p->setPen(visibilityColor(f.visibility));
        p->drawText(QRectF(rect.left() + padX, y, gutter, lineH), Qt::AlignLeft | Qt::AlignVCenter,
                    QString::fromStdString(ea::editor::visibility_symbol(f.visibility)));

        const QString sig = fm.elidedText(f.signature, Qt::ElideRight, textWidth);
        p->setPen(signatureColor);
        p->drawText(QRectF(textLeft, y, textWidth, lineH), Qt::AlignLeft | Qt::AlignVCenter, sig);
        const qreal typeLeft = textLeft + fm.horizontalAdvance(sig);
        if (!f.type.isEmpty() && typeLeft < textLeft + textWidth) {
            const qreal room = textLeft + textWidth - typeLeft;
            p->setPen(ea::gui::m3().secondary);
            p->drawText(QRectF(typeLeft, y, room, lineH), Qt::AlignLeft | Qt::AlignVCenter,
                        fm.elidedText(QStringLiteral(" : ") + f.type, Qt::ElideRight, room));
        }
        y += lineH;
    }
    return y + padY;
}

void paintCompartmentLine(QPainter* p, const QRectF& rect, qreal y) {
    p->setPen(QPen(ea::gui::m3().outlineVariant, BorderWidth));
    p->drawLine(QPointF(rect.left(), y), QPointF(rect.right(), y));
}

void paintClassifier(QPainter* p, const QRectF& rect, const SymbolContent& c) {
    const Tones t = tonesFor(c.type);
    const QPainterPath card = outlinePath(rect, c.type);
    paintShadow(p, card);
    p->fillPath(card, t.body);
    paintHeader(p, rect, card, c, t);

    const qreal hh = headerHeight(c, rect);
    const size_t totalRows = c.attributes.size() + c.operations.size();
    const bool compact = (rect.height() <= 96.0) ||
                         (totalRows > 0 && hh + 2 * PadY + static_cast<qreal>(totalRows) * LineH > rect.height());

    qreal y = rect.top() + hh;
    if (!c.attributes.empty()) y = paintFeatures(p, rect, y, c.attributes, ea::gui::m3().onSurfaceVariant, compact);
    const qreal minOpSpace = compact ? 16.0 : (PadY + LineH);
    if (!c.operations.empty() && y + minOpSpace <= rect.bottom()) {
        paintCompartmentLine(p, rect, y);
        paintFeatures(p, rect, y, c.operations, ea::gui::m3().onSurface, compact);
    }

    p->setPen(QPen(t.accent, BorderWidth));
    p->setBrush(Qt::NoBrush);
    p->drawPath(card);
}

// ---------------------------------------------------------------------------
// Other notations
// ---------------------------------------------------------------------------
QString decodeEaEntities(QString text) {
    if (!text.contains('&')) return text;
    text.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
    text.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
    text.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
    text.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
    text.replace(QStringLiteral("&apos;"), QStringLiteral("'"));
    text.replace(QStringLiteral("&nbsp;"), QStringLiteral(" "));
    static const QRegularExpression numericEntity(QStringLiteral("&#(x?[0-9A-Fa-f]+);"));
    auto it = numericEntity.globalMatch(text);
    QString out;
    int lastPos = 0;
    while (it.hasNext()) {
        const auto m = it.next();
        out += text.mid(lastPos, m.capturedStart() - lastPos);
        const QString num = m.captured(1);
        bool ok = false;
        const char32_t cp = static_cast<char32_t>(num.startsWith('x', Qt::CaseInsensitive) ? num.mid(1).toUInt(&ok, 16) : num.toUInt(&ok, 10));
        if (ok && cp > 0) {
            out += QString::fromUcs4(&cp, 1);
        } else {
            out += m.captured(0);
        }
        lastPos = m.capturedEnd();
    }
    out += text.mid(lastPos);
    return out;
}

void paintNote(QPainter* p, const QRectF& rect, const SymbolContent& c) {
    const auto& s = ea::gui::m3();
    QPainterPath sheet;
    sheet.moveTo(rect.topLeft());
    sheet.lineTo(rect.right() - NoteFold, rect.top());
    sheet.lineTo(rect.right(), rect.top() + NoteFold);
    sheet.lineTo(rect.bottomRight());
    sheet.lineTo(rect.bottomLeft());
    sheet.closeSubpath();
    paintCard(p, sheet, tonesFor(c.type).body, withAlpha(s.warning, 180));

    QPainterPath fold;
    fold.moveTo(rect.right() - NoteFold, rect.top());
    fold.lineTo(rect.right() - NoteFold, rect.top() + NoteFold);
    fold.lineTo(rect.right(), rect.top() + NoteFold);
    fold.closeSubpath();
    p->fillPath(fold, s.surfaceHigh);
    p->drawPath(fold);

    p->setFont(M3Typography::bodyMedium());
    p->setPen(s.onSurfaceVariant);
    const QString body = decodeEaEntities(c.notes.isEmpty() ? c.name : c.notes);
    p->drawText(rect.adjusted(PadX, PadY, -PadX - NoteFold / 2, -PadY), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                body);
}

void paintBoundary(QPainter* p, const QRectF& rect, const SymbolContent& c) {
    const auto& s = ea::gui::m3();
    QPainterPath frame;
    frame.addRoundedRect(rect, CornerRadius, CornerRadius);
    p->fillPath(frame, withAlpha(s.surfaceContainer, 42));
    p->setPen(QPen(withAlpha(s.outline, 190), 1.4, Qt::SolidLine));
    p->setBrush(Qt::NoBrush);
    p->drawPath(frame);

    const QString title = decodeEaEntities(c.name);
    if (!title.isEmpty()) {
        drawCentered(p, QRectF(rect.left() + PadX, rect.top() + PadY, rect.width() - 2 * PadX, NameLineH),
                     title, M3Typography::titleSmall(), s.onSurfaceVariant);
    }
}

void paintText(QPainter* p, const QRectF& rect, const SymbolContent& c) {
    const auto& s = ea::gui::m3();
    const QString body = decodeEaEntities(c.notes.isEmpty() ? c.name : c.notes);
    p->setFont(M3Typography::titleSmall());
    p->setPen(s.onSurfaceVariant);
    p->drawText(rect.adjusted(4, 2, -4, -2), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, body);
}

void paintUseCase(QPainter* p, const QRectF& rect, const SymbolContent& c) {
    const Tones t = tonesFor(c.type);
    paintCard(p, outlinePath(rect, c.type), t.body, t.accent);
    QRectF text = rect.adjusted(PadX + 4, PadY, -PadX - 4, -PadY);
    if (!c.stereotype.isEmpty()) {
        drawCentered(p, QRectF(text.left(), rect.top() + PadY + 4, text.width(), StereotypeLineH),
                     guillemets(c.stereotype), M3Typography::labelMedium(), ea::gui::m3().tertiary);
        text.setTop(text.top() + StereotypeLineH);
    }
    p->setFont(M3Typography::titleSmall());
    p->setPen(ea::gui::m3().onSurface);
    p->drawText(text, Qt::AlignCenter | Qt::TextWordWrap, c.name);
}

void paintActor(QPainter* p, const QRectF& rect, const SymbolContent& c) {
    const Tones t = tonesFor(c.type);
    paintCard(p, outlinePath(rect, c.type), t.body, withAlpha(t.accent, 160));

    constexpr qreal HeadR = 9, BodyLen = 19, ArmY = 7, ArmHalf = 12, LegDx = 10, LegLen = 14;
    const qreal cx = rect.center().x();
    const qreal headY = rect.top() + PadY + HeadR + 2;
    p->setPen(QPen(t.accent, 2.0, Qt::SolidLine, Qt::RoundCap));
    p->setBrush(t.body);
    p->drawEllipse(QPointF(cx, headY), HeadR, HeadR);
    const qreal neck = headY + HeadR, hip = neck + BodyLen;
    p->drawLine(QPointF(cx, neck), QPointF(cx, hip));
    p->drawLine(QPointF(cx - ArmHalf, neck + ArmY), QPointF(cx + ArmHalf, neck + ArmY));
    p->drawLine(QPointF(cx, hip), QPointF(cx - LegDx, hip + LegLen));
    p->drawLine(QPointF(cx, hip), QPointF(cx + LegDx, hip + LegLen));

    p->setFont(M3Typography::titleSmall());
    p->setPen(ea::gui::m3().onSurface);
    const qreal labelW = std::max(rect.width() - M3Spacing::Small, 110.0);
    const qreal labelH = std::max(rect.bottom() - hip - LegLen - M3Spacing::ExtraSmall, 32.0);
    const QRectF nameRect(cx - labelW / 2.0, hip + LegLen + M3Spacing::ExtraSmall, labelW, labelH);
    p->drawText(nameRect, Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, decodeEaEntities(c.name));
}

void paintComponent(QPainter* p, const QRectF& rect, const SymbolContent& c) {
    const Tones t = tonesFor(c.type);
    paintCard(p, outlinePath(rect, c.type), t.body, t.accent);

    // UML component glyph, top right
    constexpr qreal GlyphW = 16, GlyphH = 20, TabW = 7, TabH = 4, TabOut = 4;
    const QRectF glyph(rect.right() - PadX - GlyphW, rect.top() + PadY, GlyphW, GlyphH);
    p->setPen(QPen(t.accent, 1.2));
    p->setBrush(ea::gui::m3().surfaceHigh);
    p->drawRoundedRect(glyph, 2, 2);
    p->drawRect(QRectF(glyph.left() - TabOut, glyph.top() + 3, TabW, TabH));
    p->drawRect(QRectF(glyph.left() - TabOut, glyph.top() + 11, TabW, TabH));

    const qreal textRight = rect.width() - 2 * PadX - GlyphW - TabOut;
    qreal top = rect.top() + PadY;
    if (!c.stereotype.isEmpty()) {
        drawCentered(p, QRectF(rect.left() + PadX, top, textRight, StereotypeLineH), guillemets(c.stereotype),
                     M3Typography::labelMedium(), ea::gui::m3().tertiary);
        top += StereotypeLineH;
    }
    p->setFont(M3Typography::titleMedium());
    p->setPen(ea::gui::m3().onPrimaryContainer);
    p->drawText(QRectF(rect.left() + PadX, top, textRight, rect.bottom() - top - PadY),
                Qt::AlignCenter | Qt::TextWordWrap, c.name);
}

void paintPort(QPainter* p, const QRectF& rect, const SymbolContent& c) {
    const Tones t = tonesFor(c.type);
    paintCard(p, outlinePath(rect, c.type), t.body, t.accent);
    if (c.name.isEmpty()) return;
    constexpr qreal LabelOverhang = 20, LabelH = 16;
    drawCentered(p, QRectF(rect.left() - LabelOverhang, rect.bottom() + 2, rect.width() + 2 * LabelOverhang, LabelH),
                 c.name, M3Typography::labelMedium(), ea::gui::m3().onSurfaceVariant);
}

void paintLabelBelow(QPainter* p, const QRectF& rect, const QString& text, qreal minWidth = 112.0) {
    const QString clean = decodeEaEntities(text).trimmed();
    if (clean.isEmpty()) return;
    const qreal w = std::max(rect.width() + 40.0, minWidth);
    const QRectF labelRect(rect.center().x() - w / 2.0, rect.bottom() + 3.0, w, 54.0);
    QFont f = M3Typography::labelMedium();
    f.setPointSizeF(8.5);
    p->setFont(f);
    p->setPen(ea::gui::m3().onSurface);
    p->drawText(labelRect, Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, clean);
}

void paintInitialNode(QPainter* p, const QRectF& rect, const SymbolContent& c) {
    const auto& s = ea::gui::m3();
    const qreal d = std::min(rect.width(), rect.height());
    const QRectF circle(rect.center().x() - d / 2.0, rect.center().y() - d / 2.0, d, d);
    p->setPen(QPen(s.primary, 1.5));
    p->setBrush(s.onSurface);
    p->drawEllipse(circle);
    paintLabelBelow(p, rect, c.name, 90.0);
}

void paintFinalState(QPainter* p, const QRectF& rect, const SymbolContent& c) {
    const auto& s = ea::gui::m3();
    const qreal d = std::min(rect.width(), rect.height());
    const QRectF outer(rect.center().x() - d / 2.0, rect.center().y() - d / 2.0, d, d);
    p->setPen(QPen(s.onSurface, 1.8));
    p->setBrush(s.surfaceLow);
    p->drawEllipse(outer);
    if (c.ntype == 102) {
        // FlowFinal: circle with diagonal X
        const qreal r = d * 0.34;
        const QPointF ctr = outer.center();
        p->drawLine(ctr + QPointF(-r, -r), ctr + QPointF(r, r));
        p->drawLine(ctr + QPointF(-r, r), ctr + QPointF(r, -r));
    } else {
        // ActivityFinal / FinalState: bullseye
        const qreal inset = std::max(3.0, d * 0.24);
        p->setPen(Qt::NoPen);
        p->setBrush(s.onSurface);
        p->drawEllipse(outer.adjusted(inset, inset, -inset, -inset));
    }
    paintLabelBelow(p, rect, c.name, 90.0);
}

void paintDiamond(QPainter* p, const QRectF& rect, const SymbolContent& c) {
    const auto& s = ea::gui::m3();
    QPolygonF poly;
    poly << QPointF(rect.center().x(), rect.top())
         << QPointF(rect.right(), rect.center().y())
         << QPointF(rect.center().x(), rect.bottom())
         << QPointF(rect.left(), rect.center().y());
    QPainterPath path;
    path.addPolygon(poly);
    path.closeSubpath();
    paintCard(p, path, s.surfaceContainer, s.warning);
    paintLabelBelow(p, rect, c.name, 90.0);
}

void paintForkJoinBar(QPainter* p, const QRectF& rect, const SymbolContent& c) {
    const auto& s = ea::gui::m3();
    QPainterPath bar;
    bar.addRoundedRect(rect, 3.0, 3.0);
    p->fillPath(bar, s.onSurface);
    p->setPen(QPen(s.primary, 1.0));
    p->drawPath(bar);
    paintLabelBelow(p, rect, c.name, 80.0);
}

void paintActionOrActivity(QPainter* p, const QRectF& rect, const SymbolContent& c) {
    const auto& s = ea::gui::m3();
    const bool isContainerActivity = (c.type == ObjectType::Activity && rect.width() >= 110.0 && rect.height() >= 65.0);
    const qreal radius = std::min(16.0, std::min(rect.width(), rect.height()) * 0.35);
    QPainterPath pill;
    pill.addRoundedRect(rect, radius, radius);

    if (isContainerActivity) {
        p->fillPath(pill, withAlpha(s.surfaceContainer, 72));
        p->setPen(QPen(withAlpha(s.primary, 200), 1.4));
        p->setBrush(Qt::NoBrush);
        p->drawPath(pill);

        const QRectF titleRect(rect.left() + 8.0, rect.top() + 4.0, rect.width() - 16.0, 22.0);
        p->setFont(M3Typography::titleSmall());
        p->setPen(s.onSurface);
        p->drawText(titleRect, Qt::AlignHCenter | Qt::AlignVCenter,
                    QFontMetricsF(M3Typography::titleSmall()).elidedText(decodeEaEntities(c.name), Qt::ElideRight, titleRect.width()));
        return;
    }

    paintCard(p, pill, s.surfaceContainer, s.primary);
    QRectF textRect = rect.adjusted(4.0, 2.0, -4.0, -2.0);
    if (!c.stereotype.isEmpty()) {
        drawCentered(p, QRectF(textRect.left(), textRect.top(), textRect.width(), 12.0),
                     guillemets(c.stereotype), M3Typography::labelMedium(), s.tertiary);
        textRect.setTop(textRect.top() + 12.0);
    }
    QFont font = (rect.height() < 48.0 || rect.width() < 92.0) ? M3Typography::labelMedium() : M3Typography::titleSmall();
    if (rect.width() < 85.0) font.setPointSizeF(8.0);
    p->setFont(font);
    p->setPen(s.onSurface);
    p->drawText(textRect, Qt::AlignCenter | Qt::TextWordWrap, decodeEaEntities(c.name));
}

void paintActivityPartition(QPainter* p, const QRectF& rect, const SymbolContent& c) {
    const auto& s = ea::gui::m3();
    constexpr qreal HeaderH = 26.0;
    QPainterPath frame;
    frame.addRoundedRect(rect, 8.0, 8.0);
    p->fillPath(frame, withAlpha(s.surfaceContainer, 34));

    const QRectF header(rect.left(), rect.top(), rect.width(), std::min(HeaderH, rect.height()));
    p->save();
    p->setClipPath(frame);
    p->fillRect(header, withAlpha(s.surfaceHigh, 120));
    p->restore();

    p->setPen(QPen(withAlpha(s.secondary, 195), 1.4));
    p->setBrush(Qt::NoBrush);
    p->drawPath(frame);
    p->drawLine(QPointF(rect.left(), header.bottom()), QPointF(rect.right(), header.bottom()));

    drawCentered(p, header.adjusted(8.0, 0.0, -8.0, 0.0), decodeEaEntities(c.name),
                 M3Typography::titleSmall(), s.onSurface);
}

void paintEvent(QPainter* p, const QRectF& rect, const SymbolContent& c) {
    const auto& s = ea::gui::m3();
    const qreal notch = std::min(16.0, rect.width() * 0.24);
    QPolygonF poly;
    if (c.ntype == 1) {
        // AcceptEventAction: concave notch on the left
        poly << rect.topLeft()
             << rect.topRight()
             << rect.bottomRight()
             << rect.bottomLeft()
             << QPointF(rect.left() + notch, rect.center().y());
    } else {
        // SendSignalAction: convex arrow point on the right
        poly << rect.topLeft()
             << QPointF(rect.right() - notch, rect.top())
             << QPointF(rect.right(), rect.center().y())
             << QPointF(rect.right() - notch, rect.bottom())
             << rect.bottomLeft();
    }
    QPainterPath path;
    path.addPolygon(poly);
    path.closeSubpath();
    paintCard(p, path, s.surfaceContainer, s.warning);

    QRectF textRect = (c.ntype == 1)
                          ? rect.adjusted(notch + 1.0, 3.0, -2.0, -3.0)
                          : rect.adjusted(3.0, 3.0, -notch - 1.0, -3.0);
    QFont evFont = M3Typography::labelMedium();
    evFont.setPointSizeF(7.5);
    p->setFont(evFont);
    p->setPen(s.onSurface);
    p->drawText(textRect, Qt::AlignCenter | Qt::TextWordWrap | Qt::TextWrapAnywhere, decodeEaEntities(c.name));
}

void paintPackage(QPainter* p, const QRectF& rect, const SymbolContent& c) {
    const auto& s = ea::gui::m3();
    const qreal tabH = std::min(22.0, rect.height() * 0.28);
    const qreal tabW = std::clamp(rect.width() * 0.42, 46.0, 120.0);
    const bool largeContainer = (rect.width() >= 180.0 && rect.height() >= 130.0);

    QPainterPath tab;
    tab.addRoundedRect(QRectF(rect.left(), rect.top(), tabW, tabH + 4.0), 6.0, 6.0);
    QPainterPath body;
    body.addRoundedRect(QRectF(rect.left(), rect.top() + tabH, rect.width(), rect.height() - tabH), 8.0, 8.0);

    p->fillPath(tab, withAlpha(s.surfaceHigh, largeContainer ? 130 : 230));
    p->fillPath(body, withAlpha(s.surface, largeContainer ? 75 : 235));
    p->setPen(QPen(s.secondary, 1.2));
    p->setBrush(Qt::NoBrush);
    p->drawPath(tab);
    p->drawPath(body);

    const QRectF titleRect(rect.left() + 8.0, rect.top() + tabH + 4.0, rect.width() - 16.0,
                           largeContainer ? 22.0 : (rect.height() - tabH - 8.0));
    p->setFont(M3Typography::titleSmall());
    p->setPen(s.onSurface);
    p->drawText(titleRect, (largeContainer ? (Qt::AlignLeft | Qt::AlignTop) : Qt::AlignCenter) | Qt::TextWordWrap,
                decodeEaEntities(c.name));
}

bool paintRobustnessIcon(QPainter* p, const QRectF& rect, const SymbolContent& c) {
    const QString st = c.stereotype.trimmed().toLower();
    if (st != "boundary" && st != "control" && st != "entity") return false;
    if (!c.attributes.empty() || !c.operations.empty()) return false;

    const auto& s = ea::gui::m3();
    const qreal glyphH = std::max(24.0, std::min(rect.height() * 0.66, rect.width() * 0.78));
    const qreal r = glyphH * 0.44;
    const qreal cx = rect.center().x() + (st == "boundary" ? r * 0.25 : 0.0);
    const qreal cy = rect.top() + r + 5.0;

    if (st == "boundary") {
        // ⊢○ Boundary icon (UI / interface)
        const QColor accent = s.primary;
        p->setPen(QPen(accent, 2.0, Qt::SolidLine, Qt::RoundCap));
        p->setBrush(s.surfaceContainer);
        const qreal barX = cx - r - std::max(7.0, r * 0.55);
        p->drawLine(QPointF(barX, cy - r * 0.85), QPointF(barX, cy + r * 0.85));
        p->drawLine(QPointF(barX, cy), QPointF(cx - r, cy));
        p->drawEllipse(QPointF(cx, cy), r, r);
    } else if (st == "control") {
        // ↻ Control icon (controller / logic)
        const QColor accent = s.warning;
        p->setPen(QPen(accent, 2.0, Qt::SolidLine, Qt::RoundCap));
        p->setBrush(s.surfaceContainer);
        p->drawEllipse(QPointF(cx, cy), r, r);
        const QPointF top(cx, cy - r);
        p->drawLine(top, top + QPointF(5.5, -4.5));
        p->drawLine(top, top + QPointF(5.5, 4.5));
    } else {
        // ◯̲ Entity icon (domain data)
        const QColor accent = s.success;
        p->setPen(QPen(accent, 2.0, Qt::SolidLine, Qt::RoundCap));
        p->setBrush(s.surfaceContainer);
        p->drawEllipse(QPointF(cx, cy), r, r);
        p->drawLine(QPointF(cx - r * 1.15, cy + r + 2.0), QPointF(cx + r * 1.15, cy + r + 2.0));
    }

    // Label below the robustness glyph (compact width so adjacent robustness icons don't overlap)
    const qreal labelW = std::max(rect.width() + 44.0, 96.0);
    const qreal labelTop = cy + r + 5.0;
    const QRectF labelRect(rect.center().x() - labelW / 2.0, labelTop, labelW, 56.0);
    QFont robFont = M3Typography::labelMedium();
    robFont.setPointSizeF(8.0);
    p->setFont(robFont);
    p->setPen(s.onSurface);
    p->drawText(labelRect, Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, decodeEaEntities(c.name));
    return true;
}

} // namespace

void paintSymbol(QPainter* painter, const QRectF& rect, const SymbolContent& content) {
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::TextAntialiasing, true);
    if ((content.type == ObjectType::Object || content.type == ObjectType::Class) &&
        paintRobustnessIcon(painter, rect, content)) {
        return;
    }
    switch (content.type) {
        case ObjectType::Note: paintNote(painter, rect, content); break;
        case ObjectType::Boundary: paintBoundary(painter, rect, content); break;
        case ObjectType::Text: paintText(painter, rect, content); break;
        case ObjectType::UseCase: paintUseCase(painter, rect, content); break;
        case ObjectType::Actor: paintActor(painter, rect, content); break;
        case ObjectType::Component: paintComponent(painter, rect, content); break;
        case ObjectType::Port: paintPort(painter, rect, content); break;
        case ObjectType::Package: paintPackage(painter, rect, content); break;
        case ObjectType::InitialNode: paintInitialNode(painter, rect, content); break;
        case ObjectType::FinalState: paintFinalState(painter, rect, content); break;
        case ObjectType::Decision:
        case ObjectType::MergeNode: paintDiamond(painter, rect, content); break;
        case ObjectType::ForkNode:
        case ObjectType::JoinNode: paintForkJoinBar(painter, rect, content); break;
        case ObjectType::Action:
        case ObjectType::Activity: paintActionOrActivity(painter, rect, content); break;
        case ObjectType::ActivityPartition: paintActivityPartition(painter, rect, content); break;
        case ObjectType::Event: paintEvent(painter, rect, content); break;
        default: paintClassifier(painter, rect, content); break;
    }
}

void paintSelectionGlow(QPainter* painter, const QRectF& rect, ObjectType type, bool selected, qreal hover) {
    if (!selected && hover <= 0.01) return;
    constexpr qreal RingGap = 3.0, AuraWidth = 5.0, SelectedWidth = 1.8, HoverWidth = 1.2;
    constexpr int AuraAlpha = 45;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setBrush(Qt::NoBrush);
    const QRectF ring = rect.adjusted(-RingGap, -RingGap, RingGap, RingGap);
    QPainterPath path;
    if (type == ObjectType::UseCase) {
        path.addEllipse(ring);
    } else {
        path.addRoundedRect(ring, CornerRadius + RingGap, CornerRadius + RingGap);
    }

    const auto g = ea::gui::DarkTheme::geminiGradient();
    if (selected) {
        painter->setPen(QPen(withAlpha(g.start, AuraAlpha), AuraWidth));
        painter->drawPath(path);
    }
    QLinearGradient gemini(ring.topLeft(), ring.bottomRight());
    gemini.setColorAt(0.0, g.start);
    gemini.setColorAt(0.5, g.middle);
    gemini.setColorAt(1.0, g.end);
    painter->setOpacity(selected ? 1.0 : hover);
    painter->setPen(QPen(QBrush(gemini), selected ? SelectedWidth : HoverWidth));
    painter->drawPath(path);
    painter->restore();
}

void paintHandles(QPainter* painter, const std::vector<QPointF>& centers) {
    constexpr qreal Radius = 4.5;
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(QPen(ea::gui::m3().surfaceLowest, 1.5));
    painter->setBrush(ea::gui::m3().primary);
    for (const auto& c : centers) painter->drawEllipse(c, Radius, Radius);
    painter->restore();
}

} // namespace uml_symbols
