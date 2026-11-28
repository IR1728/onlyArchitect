#pragma once

// Painting of interaction-diagram notation (UML 2.5 §17): lifelines with their execution
// specifications, combined fragments and interaction uses. Item-local coordinates.

#include <QColor>
#include <QRectF>
#include <QString>
#include <vector>

class QPainter;

namespace sequence_symbols {

// Execution specification in item-local y
struct ActivationBar {
    double top;
    double bottom;
    int depth;
};

enum class LifelineHead { Box, Actor };

inline constexpr double HeadHeight = 44.0;      // box head
inline constexpr double ActorHeadHeight = 70.0; // stick figure + name
inline constexpr double ActivationWidth = 12.0;
inline constexpr double ActivationNestOffset = 6.0;

double headHeight(LifelineHead head);
void paintLifeline(QPainter* p, const QRectF& rect, const QString& title, LifelineHead head,
                   const std::vector<ActivationBar>& activations, bool selected);
// Combined fragment: frame with the operator pentagon in the corner and the guard beneath it
void paintCombinedFragment(QPainter* p, const QRectF& rect, const QString& op, const QString& guard, bool selected);
// Interaction use: frame labelled "ref" with the referenced interaction centred
void paintInteractionUse(QPainter* p, const QRectF& rect, const QString& name, bool selected);

} // namespace sequence_symbols
