#include "ea/xmi/Xmi.hpp"
#include <unordered_map>

namespace ea::xmi {

std::string uml_metaclass(ObjectType type) {
    switch (type) {
        case ObjectType::Package: return "uml:Package";
        case ObjectType::Class: return "uml:Class";
        case ObjectType::Interface: return "uml:Interface";
        case ObjectType::Enumeration: return "uml:Enumeration";
        case ObjectType::DataType: return "uml:DataType";
        case ObjectType::PrimitiveType: return "uml:PrimitiveType";
        case ObjectType::Component: return "uml:Component";
        case ObjectType::Node: return "uml:Node";
        case ObjectType::Artifact: return "uml:Artifact";
        case ObjectType::Device: return "uml:Device";
        case ObjectType::ExecutionEnvironment: return "uml:ExecutionEnvironment";
        case ObjectType::UseCase: return "uml:UseCase";
        case ObjectType::Actor: return "uml:Actor";
        case ObjectType::Activity: return "uml:Activity";
        case ObjectType::Action: return "uml:OpaqueAction";
        case ObjectType::State: return "uml:State";
        case ObjectType::StateMachine: return "uml:StateMachine";
        case ObjectType::InitialNode: return "uml:InitialNode";
        case ObjectType::FinalState: return "uml:FinalState";
        case ObjectType::Decision: return "uml:DecisionNode";
        case ObjectType::MergeNode: return "uml:MergeNode";
        case ObjectType::ForkNode: return "uml:ForkNode";
        case ObjectType::JoinNode: return "uml:JoinNode";
        case ObjectType::ObjectNode: return "uml:CentralBufferNode";
        case ObjectType::ActivityPartition: return "uml:ActivityPartition";
        case ObjectType::Event: return "uml:AcceptEventAction";
        case ObjectType::Port: return "uml:Port";
        case ObjectType::Object: return "uml:InstanceSpecification";
        case ObjectType::Lifeline: return "uml:Lifeline";
        case ObjectType::InteractionFragment: return "uml:CombinedFragment";
        case ObjectType::InteractionOccurrence: return "uml:InteractionUse";
        case ObjectType::Note:
        case ObjectType::Text: return "uml:Comment";
        // No UML metaclass: a class; the extension keeps the real kind (Requirement, Boundary, Screen, ...)
        default: return "uml:Class";
    }
}

ObjectType object_type_for(std::string_view umlType) {
    static const std::unordered_map<std::string_view, ObjectType> kinds = {
        {"uml:Class", ObjectType::Class},
        {"uml:AssociationClass", ObjectType::Class},
        {"uml:Interface", ObjectType::Interface},
        {"uml:Enumeration", ObjectType::Enumeration},
        {"uml:DataType", ObjectType::DataType},
        {"uml:PrimitiveType", ObjectType::PrimitiveType},
        {"uml:Component", ObjectType::Component},
        {"uml:Node", ObjectType::Node},
        {"uml:Artifact", ObjectType::Artifact},
        {"uml:Device", ObjectType::Device},
        {"uml:ExecutionEnvironment", ObjectType::ExecutionEnvironment},
        {"uml:UseCase", ObjectType::UseCase},
        {"uml:Actor", ObjectType::Actor},
        {"uml:Activity", ObjectType::Activity},
        {"uml:OpaqueAction", ObjectType::Action},
        {"uml:Action", ObjectType::Action},
        {"uml:CallBehaviorAction", ObjectType::Action},
        {"uml:State", ObjectType::State},
        {"uml:StateMachine", ObjectType::StateMachine},
        {"uml:InitialNode", ObjectType::InitialNode},
        {"uml:FinalState", ObjectType::FinalState},
        {"uml:ActivityFinalNode", ObjectType::FinalState},
        {"uml:FlowFinalNode", ObjectType::FinalState},
        {"uml:Pseudostate", ObjectType::State},
        {"uml:DecisionNode", ObjectType::Decision},
        {"uml:MergeNode", ObjectType::MergeNode},
        {"uml:ForkNode", ObjectType::ForkNode},
        {"uml:JoinNode", ObjectType::JoinNode},
        {"uml:CentralBufferNode", ObjectType::ObjectNode},
        {"uml:DataStoreNode", ObjectType::ObjectNode},
        {"uml:ActivityPartition", ObjectType::ActivityPartition},
        {"uml:SendSignalAction", ObjectType::Event},
        {"uml:AcceptEventAction", ObjectType::Event},
        {"uml:SignalEvent", ObjectType::Event},
        {"uml:TimeEvent", ObjectType::Event},
        {"uml:Port", ObjectType::Port},
        {"uml:InstanceSpecification", ObjectType::Object},
        {"uml:Object", ObjectType::Object},
        {"uml:Lifeline", ObjectType::Lifeline},
        {"uml:CombinedFragment", ObjectType::InteractionFragment},
        {"uml:InteractionUse", ObjectType::InteractionOccurrence},
        {"uml:Comment", ObjectType::Note},
    };
    auto it = kinds.find(umlType);
    return it != kinds.end() ? it->second : ObjectType::Unknown;
}

} // namespace ea::xmi
