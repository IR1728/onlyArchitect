#pragma once

// Interaction (sequence diagram) semantics on top of EA's storage: message sorts, vertical order,
// numbering and execution specifications (activation bars). Pure functions, no UI.
//
// EA encoding of a message (t_connector, Connector_Type "Sequence"):
//   PDATA1 "Synchronous" | "Asynchronous"   PDATA4 "1" = reply      SubType "New" / "Delete"
//   PtStartY = PtEndY = -y (EA coordinates grow negative downwards)   SeqNo = order on the diagram

#include <string>
#include <vector>
#include "ea/core/Types.hpp"
#include "ea/model/Connector.hpp"

namespace ea::sequence {

enum class MessageSort { Synchronous, Asynchronous, Reply, Create, Delete };

MessageSort message_sort(const Connector& message);
void set_message_sort(Connector& message, MessageSort sort);
std::string to_string(MessageSort sort);

// Vertical position in diagram coordinates (y grows downwards, as on screen)
int message_y(const Connector& message);
void set_message_y(Connector& message, int y);

// "3: login()"; replies and messages without a name still show their number
std::string message_label(const Connector& message);

// Elements that act as lifelines on a sequence diagram (messages start and end on them)
bool is_lifeline_kind(ObjectType type);
// Symbols that are drawn behind lifelines and do not take messages (combined fragments, ref)
bool is_interaction_frame(ObjectType type);

// Interaction operator of a combined fragment (EA NType): "alt", "opt", "loop", ...
std::string fragment_operator(int ntype);

struct SymbolSize {
    int width;
    int height;
};
// Size of a new symbol: lifelines are tall on interaction diagrams, fragments wide
SymbolSize default_symbol_size(ObjectType element, DiagramType diagram);

// --- Ordering ---------------------------------------------------------------------------------

struct MessagePosition {
    int y{0};
    int sequenceNo{0};
    bool operator==(const MessagePosition&) const = default;
};

struct MessagePlacement {
    int connectorId{0};
    MessagePosition before;
    MessagePosition after;
};

// Places message `connectorId` at `y` among `messages` (the messages of one diagram; it may or may
// not already be among them) and renumbers all of them top to bottom (1..n). Returns every message
// whose position or number changes, including the placed one. A message placed at the same height as
// another goes after it.
std::vector<MessagePlacement> place_message(const std::vector<const Connector*>& messages, int connectorId, int y);

// --- Execution specifications -----------------------------------------------------------------

struct Activation {
    int lifelineId{0};
    int top{0};
    int bottom{0};
    int depth{0}; // nesting level on its lifeline (0 = outermost), for the horizontal offset
};

// Default height of an activation that no reply closes
inline constexpr int OpenActivationHeight = 30;

// Activation bars implied by the messages (UML 2.5 §17.2.3): a synchronous call starts one on the
// receiver; the receiver's reply to that caller ends it (and anything nested in it). Calls without a
// reply get OpenActivationHeight.
std::vector<Activation> compute_activations(const std::vector<const Connector*>& messages);

} // namespace ea::sequence
