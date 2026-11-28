#include "ea/diagram/SequenceModel.hpp"
#include <algorithm>
#include <unordered_map>

namespace ea::sequence {

namespace {

constexpr int SortSlot = 1;   // PDATA1
constexpr int ReturnSlot = 4; // PDATA4
constexpr const char* kSynchronous = "Synchronous";
constexpr const char* kAsynchronous = "Asynchronous";
constexpr const char* kCreate = "New";
constexpr const char* kDelete = "Delete";

} // namespace

MessageSort message_sort(const Connector& m) {
    if (m.GetSubtype() == kCreate) return MessageSort::Create;
    if (m.GetSubtype() == kDelete) return MessageSort::Delete;
    if (m.GetPData(ReturnSlot) == "1") return MessageSort::Reply;
    if (m.GetPData(SortSlot) == kAsynchronous) return MessageSort::Asynchronous;
    return MessageSort::Synchronous;
}

void set_message_sort(Connector& m, MessageSort sort) {
    m.SetSubtype(sort == MessageSort::Create ? kCreate : sort == MessageSort::Delete ? kDelete : "");
    m.SetPData(SortSlot, sort == MessageSort::Asynchronous ? kAsynchronous : kSynchronous);
    m.SetPData(ReturnSlot, sort == MessageSort::Reply ? "1" : "0");
    if (m.GetPData(3).empty()) m.SetPData(3, "Call");           // EA: message kind
    if (m.GetPData(2).empty()) m.SetPData(2, "retval=void;");   // EA: return value
}

std::string to_string(MessageSort sort) {
    switch (sort) {
        case MessageSort::Asynchronous: return "asynchCall";
        case MessageSort::Reply: return "reply";
        case MessageSort::Create: return "createMessage";
        case MessageSort::Delete: return "deleteMessage";
        default: return "synchCall";
    }
}

int message_y(const Connector& m) {
    return -m.GetStartY();
}

void set_message_y(Connector& m, int y) {
    m.SetStartY(-y);
    m.SetEndY(-y);
}

std::string message_label(const Connector& m) {
    std::string label = m.GetSequenceNo() > 0 ? std::to_string(m.GetSequenceNo()) + ":" : "";
    if (!m.GetName().empty()) label += (label.empty() ? "" : " ") + m.GetName();
    return label;
}

bool is_lifeline_kind(ObjectType type) {
    switch (type) {
        case ObjectType::Lifeline:
        case ObjectType::Object:
        case ObjectType::Actor:
        case ObjectType::Class:
        case ObjectType::Component:
        case ObjectType::Interface:
        case ObjectType::Node:
        case ObjectType::Device:
        case ObjectType::ExecutionEnvironment:
        case ObjectType::TimeLine: return true;
        default: return false;
    }
}

bool is_interaction_frame(ObjectType type) {
    return type == ObjectType::InteractionFragment || type == ObjectType::InteractionOccurrence;
}

std::string fragment_operator(int ntype) {
    // EA's NType order for InteractionFragment (UML 2.5 InteractionOperatorKind)
    static constexpr const char* kOperators[] = {"alt", "opt", "break", "par", "loop", "critical", "neg",
                                                 "assert", "strict", "seq", "ignore", "consider"};
    constexpr int count = static_cast<int>(sizeof(kOperators) / sizeof(kOperators[0]));
    return ntype >= 0 && ntype < count ? kOperators[ntype] : "alt";
}

SymbolSize default_symbol_size(ObjectType element, DiagramType diagram) {
    const bool interaction = diagram == DiagramType::Sequence || diagram == DiagramType::Timing;
    if (interaction && is_lifeline_kind(element)) return {110, 420};
    if (element == ObjectType::InteractionFragment) return {320, 160};
    if (element == ObjectType::InteractionOccurrence) return {260, 60};
    return {170, 100};
}

std::vector<MessagePlacement> place_message(const std::vector<const Connector*>& messages, int connectorId, int y) {
    struct Entry {
        int id;
        MessagePosition before;
        int y;
        int rank; // tie-break at equal height: existing order first, the placed message last
    };
    std::vector<Entry> entries;
    entries.reserve(messages.size() + 1);
    bool present = false;
    for (const auto* m : messages) {
        if (!m) continue;
        const MessagePosition before{message_y(*m), m->GetSequenceNo()};
        const bool placed = m->GetConnectorID() == connectorId;
        present = present || placed;
        entries.push_back({m->GetConnectorID(), before, placed ? y : before.y, placed ? 1 : 0});
    }
    if (!present) entries.push_back({connectorId, {y, 0}, y, 1});

    std::stable_sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
        if (a.y != b.y) return a.y < b.y;
        if (a.rank != b.rank) return a.rank < b.rank;
        return a.before.sequenceNo < b.before.sequenceNo;
    });

    std::vector<MessagePlacement> changes;
    for (size_t i = 0; i < entries.size(); ++i) {
        const MessagePosition after{entries[i].y, static_cast<int>(i) + 1};
        if (!(after == entries[i].before) || entries[i].id == connectorId) {
            changes.push_back({entries[i].id, entries[i].before, after});
        }
    }
    return changes;
}

std::vector<Activation> compute_activations(const std::vector<const Connector*>& messages) {
    std::vector<const Connector*> ordered;
    ordered.reserve(messages.size());
    for (const auto* m : messages) if (m) ordered.push_back(m);
    std::stable_sort(ordered.begin(), ordered.end(), [](const Connector* a, const Connector* b) {
        const int ya = message_y(*a), yb = message_y(*b);
        return ya != yb ? ya < yb : a->GetSequenceNo() < b->GetSequenceNo();
    });

    struct Open {
        Activation activation;
        int callerId;
    };
    std::vector<Activation> done;
    std::unordered_map<int, std::vector<Open>> open; // per lifeline, innermost last
    for (const auto* m : ordered) {
        const int y = message_y(*m);
        switch (message_sort(*m)) {
            case MessageSort::Synchronous: {
                auto& stack = open[m->GetSupplierID()];
                stack.push_back({{m->GetSupplierID(), y, y, static_cast<int>(stack.size())}, m->GetClientID()});
                break;
            }
            case MessageSort::Reply: {
                // The reply X→Y ends the activation Y's call opened on X, and whatever is nested in it
                auto it = open.find(m->GetClientID());
                if (it == open.end() || it->second.empty()) break; // a reply nobody waited for
                auto& stack = it->second;
                size_t k = stack.size();
                while (k > 0 && stack[k - 1].callerId != m->GetSupplierID()) --k;
                const size_t from = k > 0 ? k - 1 : stack.size() - 1; // no matching call: the innermost
                for (size_t i = stack.size(); i > from; --i) {
                    auto a = stack[i - 1].activation;
                    a.bottom = y;
                    done.push_back(a);
                }
                stack.resize(from);
                break;
            }
            default: break; // asynchronous, create and delete messages do not start a blocking activation
        }
    }
    for (auto& [id, stack] : open) {
        for (auto& o : stack) {
            o.activation.bottom = o.activation.top + OpenActivationHeight;
            done.push_back(o.activation);
        }
    }
    std::sort(done.begin(), done.end(), [](const Activation& a, const Activation& b) {
        return a.lifelineId != b.lifelineId ? a.lifelineId < b.lifelineId : a.top < b.top;
    });
    return done;
}

} // namespace ea::sequence
