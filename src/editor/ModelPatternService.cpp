#include "ea/editor/ModelPatternService.hpp"
#include "ea/model/Attribute.hpp"
#include "ea/model/Method.hpp"
#include <algorithm>
#include <cctype>

namespace ea::editor {

namespace {

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

bool contains(const std::string& haystack, const std::string& needle) {
    return lower(haystack).find(lower(needle)) != std::string::npos;
}

DiagramType diagramTypeForCategory(const std::string& category, const std::string& patternName) {
    const std::string text = category + " " + patternName;
    if (contains(text, "use case")) return DiagramType::UseCase;
    if (contains(text, "sequence")) return DiagramType::Sequence;
    if (contains(text, "activity")) return DiagramType::Activity;
    if (contains(text, "state machine")) return DiagramType::Statechart;
    if (contains(text, "component")) return DiagramType::Component;
    if (contains(text, "package")) return DiagramType::Package;
    if (contains(text, "object")) return DiagramType::Object;
    if (contains(text, "composite")) return DiagramType::CompositeStructure;
    if (contains(text, "communication")) return DiagramType::Communication;
    if (contains(text, "deployment")) return DiagramType::Deployment;
    return DiagramType::Logical;
}

struct PatternBuilder {
    Repository& repo;
    std::shared_ptr<Diagram> diag;
    int pkgId{0};
    size_t elemCount{0};
    size_t connCount{0};

    std::shared_ptr<Element> addBox(const std::string& name, ObjectType type, int left, int top, int width, int height,
                                    const std::string& stereotype = "", const std::string& notes = "", int ntype = 0) {
        Element e;
        e.SetName(name);
        e.SetType(type);
        e.SetPackageID(pkgId);
        e.SetStereotype(stereotype);
        e.SetNotes(notes);
        if (ntype != 0) {
            e.SetNType(ntype);
        } else if (type == ObjectType::InitialNode) {
            e.SetNType(diag && diag->GetType() == DiagramType::Statechart ? 3 : 100);
        } else if (type == ObjectType::FinalState) {
            e.SetNType(diag && diag->GetType() == DiagramType::Statechart ? 4 : 101);
        }
        auto saved = repo.AddElement(e);
        if (!saved) return nullptr;

        auto dobj = std::make_shared<DiagramObject>();
        dobj->SetDiagramID(diag->GetDiagramID());
        dobj->SetElementID(saved->GetElementID());
        dobj->SetLeft(left);
        dobj->SetTop(-top);
        dobj->SetRight(left + width);
        dobj->SetBottom(-(top + height));
        repo.AddElementToDiagram(*diag, dobj);
        ++elemCount;
        return saved;
    }

    std::shared_ptr<Connector> connect(const std::shared_ptr<Element>& from, const std::shared_ptr<Element>& to,
                                       ConnectorType type, const std::string& name = "",
                                       const std::string& stereotype = "", int seqNo = 0, int yPos = 0) {
        if (!from || !to) return nullptr;
        Connector c;
        c.SetClientID(from->GetElementID());
        c.SetSupplierID(to->GetElementID());
        c.SetType(type);
        c.SetName(name);
        c.SetStereotype(stereotype);
        if (seqNo > 0) {
            c.SetSequenceNo(seqNo);
            c.SetDiagramID(diag->GetDiagramID());
            c.SetStartY(yPos);
            c.SetEndY(yPos);
        }
        auto saved = repo.AddConnector(c);
        if (!saved) return nullptr;

        auto link = std::make_shared<DiagramLink>();
        link->SetDiagramID(diag->GetDiagramID());
        link->SetConnectorID(saved->GetConnectorID());
        repo.AddConnectorToDiagram(*diag, link);
        ++connCount;
        return saved;
    }
};

} // namespace

PatternResult ModelPatternService::instantiatePattern(Repository& repo, int parentPackageId,
                                                      const std::string& category, const std::string& patternName) {
    PatternResult result;
    if (!repo.IsOpen()) {
        result.error = "No hay un repositorio abierto. Abre o crea un proyecto (.qea) primero.";
        return result;
    }

    const std::string name = patternName.empty() ? "Nuevo Modelo UML" : patternName;

    std::shared_ptr<Package> pkg;
    if (parentPackageId > 0 && repo.GetPackageByID(parentPackageId)) {
        pkg = repo.CreatePackage(parentPackageId, name);
    } else if (repo.GetModels().size() > 0 && repo.GetModels().GetAt(0)) {
        pkg = repo.CreatePackage(repo.GetModels().GetAt(0)->GetPackageID(), name);
    } else {
        pkg = repo.CreateModel(name);
    }

    if (!pkg) {
        result.error = "No se pudo crear el paquete para el patrón: " + repo.GetLastError();
        return result;
    }

    const DiagramType dType = diagramTypeForCategory(category, name);
    auto diag = repo.CreateDiagram(pkg->GetPackageID(), name, dType);
    if (!diag) {
        result.error = "No se pudo crear el diagrama para el patrón: " + repo.GetLastError();
        return result;
    }

    PatternBuilder b{repo, diag, pkg->GetPackageID(), 0, 0};

    switch (dType) {
        case DiagramType::UseCase: {
            auto boundary = b.addBox("Sistema", ObjectType::Boundary, 240, 60, 380, 300);
            (void)boundary;
            auto actor = b.addBox("Usuario", ObjectType::Actor, 70, 140, 70, 100);
            auto uc1 = b.addBox("Caso de Uso Principal", ObjectType::UseCase, 290, 120, 170, 70);
            auto uc2 = b.addBox("Caso de Uso Auxiliar", ObjectType::UseCase, 420, 240, 170, 70);
            b.connect(actor, uc1, ConnectorType::Association);
            if (contains(name, "extend")) {
                b.connect(uc2, uc1, ConnectorType::UseCaseLink, "", "extend");
            } else {
                b.connect(uc1, uc2, ConnectorType::UseCaseLink, "", "include");
            }
            break;
        }
        case DiagramType::Sequence: {
            auto client = b.addBox("Cliente", ObjectType::Lifeline, 80, 60, 110, 340);
            auto service = b.addBox("Servicio", ObjectType::Lifeline, 300, 60, 110, 340);
            auto repoObj = b.addBox("Repositorio", ObjectType::Lifeline, 520, 60, 110, 340);
            b.connect(client, service, ConnectorType::Message, "solicitar()", "", 1, 150);
            b.connect(service, repoObj, ConnectorType::Message, "consultar()", "", 2, 210);
            b.connect(repoObj, service, ConnectorType::Message, "resultado", "", 3, 270);
            b.connect(service, client, ConnectorType::Message, "respuesta", "", 4, 330);
            break;
        }
        case DiagramType::Activity: {
            auto init = b.addBox("", ObjectType::InitialNode, 80, 145, 30, 30);
            auto act1 = b.addBox("Recibir Entrada", ObjectType::Action, 170, 130, 150, 60);
            auto dec = b.addBox("Valido?", ObjectType::Decision, 380, 135, 50, 50);
            auto act2 = b.addBox("Procesar", ObjectType::Action, 490, 130, 150, 60);
            auto fin = b.addBox("", ObjectType::FinalState, 700, 145, 30, 30);
            b.connect(init, act1, ConnectorType::ControlFlow);
            b.connect(act1, dec, ConnectorType::ControlFlow);
            b.connect(dec, act2, ConnectorType::ControlFlow, "[si]");
            b.connect(act2, fin, ConnectorType::ControlFlow);
            break;
        }
        case DiagramType::Statechart: {
            auto init = b.addBox("", ObjectType::InitialNode, 80, 155, 30, 30);
            auto st1 = b.addBox("Inicializado", ObjectType::State, 170, 130, 150, 70);
            auto st2 = b.addBox("En Proceso", ObjectType::State, 390, 130, 150, 70);
            auto fin = b.addBox("", ObjectType::FinalState, 610, 155, 30, 30);
            b.connect(init, st1, ConnectorType::Transition);
            b.connect(st1, st2, ConnectorType::Transition, "iniciar");
            b.connect(st2, fin, ConnectorType::Transition, "finalizar");
            break;
        }
        case DiagramType::Component: {
            auto ui = b.addBox("Cliente UI", ObjectType::Component, 80, 120, 180, 80, "component");
            auto api = b.addBox("Servicio Core", ObjectType::Component, 340, 120, 180, 80, "component");
            auto db = b.addBox("Persistencia", ObjectType::Component, 600, 120, 180, 80, "component");
            b.connect(ui, api, ConnectorType::Dependency, "usa");
            b.connect(api, db, ConnectorType::Dependency, "consulta");
            break;
        }
        default: { // Logical / Class / Package / Object
            auto base = b.addBox("EntidadBase", ObjectType::Class, 240, 60, 190, 110);
            if (base) {
                auto idAttr = std::make_shared<Attribute>();
                idAttr->SetName("id");
                idAttr->SetType("int");
                idAttr->SetVisibility(Scope::Private);
                base->GetAttributes().AddExisting(idAttr);
                repo.SaveElement(*base);
            }
            auto child = b.addBox("EntidadConcreta", ObjectType::Class, 240, 240, 190, 110);
            if (child) {
                auto nameAttr = std::make_shared<Attribute>();
                nameAttr->SetName("nombre");
                nameAttr->SetType("std::string");
                nameAttr->SetVisibility(Scope::Private);
                child->GetAttributes().AddExisting(nameAttr);
                auto op = std::make_shared<Method>();
                op->SetName("validar");
                op->SetReturnType("bool");
                op->SetVisibility(Scope::Public);
                child->GetMethods().AddExisting(op);
                repo.SaveElement(*child);
            }
            b.connect(child, base, ConnectorType::Generalization);
            break;
        }
    }

    result.ok = true;
    result.packageId = pkg->GetPackageID();
    result.diagramId = diag->GetDiagramID();
    result.elementsCreated = b.elemCount;
    result.connectorsCreated = b.connCount;
    return result;
}

} // namespace ea::editor
