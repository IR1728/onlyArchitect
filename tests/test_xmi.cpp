#ifdef NDEBUG
#undef NDEBUG
#endif
// XMI interchange (no Qt): XML reader/writer, UML 2.5.1 export (well-formed, every reference
// resolves), lossless round trip including diagrams, import of foreign (EA-style) XMI, errors,
// and EA's sample model at full size.
#include <cassert>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <cstdlib>
#include <fstream>
#include <set>
#include <sstream>
#include "ea/editor/FeatureText.hpp"
#include <unordered_set>
#include "ea/diagram/SequenceModel.hpp"
#include "ea/xmi/Xmi.hpp"
#include "ea/xml/Xml.hpp"

namespace fs = std::filesystem;
using namespace ea;

namespace {

std::string count(Repository& repo, const std::string& sql) {
    const std::string xml = repo.SQLQuery(sql);
    const auto a = xml.find("<n>"), b = xml.find("</n>");
    return a == std::string::npos ? "" : xml.substr(a + 3, b - a - 3);
}

void collect(const xml::Node& n, bool inExtension, std::unordered_set<std::string>& ids, std::vector<std::string>& refs,
             std::multiset<std::string>& types) {
    const bool ext = inExtension || n.name == "xmi:Extension";
    if (const auto* id = n.attribute("xmi:id")) assert(ids.insert(*id).second && "duplicate xmi:id");
    if (!ext) if (const auto* t = n.attribute("xmi:type")) types.insert(*t);
    static const std::set<std::string> single = {"general", "contract", "addition", "extendedCase", "association",
                                                 "informationSource", "informationTarget", "client", "supplier",
                                                 "subject", "xmi:idref", "package"};
    static const std::set<std::string> lists = {"memberEnd", "annotatedElement"};
    for (const auto& [k, v] : n.attributes) {
        const bool baseRef = k.rfind("base_", 0) == 0;
        if (single.count(k) || baseRef || (k == "type" && !ext)) refs.push_back(v);
        if (lists.count(k)) {
            std::istringstream in(v);
            for (std::string r; in >> r;) refs.push_back(r);
        }
    }
    for (const auto& c : n.children) collect(*c, ext, ids, refs, types);
}

// Every reference in an exported document points at an element of the document
std::multiset<std::string> check_integrity(const std::string& document) {
    auto parsed = xml::parse(document);
    assert(parsed.ok());
    std::unordered_set<std::string> ids;
    std::vector<std::string> refs;
    std::multiset<std::string> types;
    collect(*parsed.root, false, ids, refs, types);
    for (const auto& r : refs) {
        if (!ids.count(r)) std::cerr << "    unresolved reference: " << r << "\n";
        assert(ids.count(r) && "dangling reference in exported XMI");
    }
    return types;
}

std::shared_ptr<Element> add(Repository& repo, int pkg, ObjectType type, const std::string& name, const std::string& stereotype = "") {
    Element e;
    e.SetType(type);
    e.SetName(name);
    e.SetStereotype(stereotype);
    e.SetPackageID(pkg);
    return repo.AddElement(e);
}

std::shared_ptr<Connector> link(Repository& repo, ConnectorType type, int from, int to, const std::string& stereotype = "") {
    Connector c;
    c.SetType(type);
    c.SetClientID(from);
    c.SetSupplierID(to);
    c.SetStereotype(stereotype);
    return repo.AddConnector(c);
}

std::shared_ptr<Element> byName(Repository& repo, const std::string& name) {
    const std::string xml = repo.SQLQuery("SELECT Object_ID AS n FROM t_object WHERE Name='" + name + "' AND Object_Type<>'Package'");
    const auto a = xml.find("<n>");
    assert(a != std::string::npos && name.c_str());
    return repo.GetElementByID(std::stoi(xml.substr(a + 3)));
}

std::shared_ptr<Connector> connectorBetween(Repository& repo, const std::string& from, const std::string& to, ConnectorType type) {
    auto a = byName(repo, from);
    for (const auto& c : a->GetConnectors()) {
        auto other = repo.GetElementByID(c->GetSupplierID());
        if (c->GetClientID() == a->GetElementID() && other && other->GetName() == to && c->GetType() == type) return c;
    }
    return nullptr;
}

const char* kForeignXmi = R"XMI(<?xml version="1.0" encoding="windows-1252"?>
<!-- written by another tool -->
<xmi:XMI xmi:version="2.1" xmlns:uml="http://schema.omg.org/spec/UML/2.1" xmlns:xmi="http://schema.omg.org/spec/XMI/2.1"
         xmlns:thecustomprofile="http://www.sparxsystems.com/profiles/thecustomprofile/1.0">
  <xmi:Documentation exporter="Enterprise Architect" exporterVersion="6.5"/>
  <uml:Model xmi:type="uml:Model" name="EA_Model" visibility="public">
    <packagedElement xmi:type="uml:Package" xmi:id="EAPK_1" name="Banca">
      <packagedElement xmi:type="uml:Class" xmi:id="EAID_C" name="Cuenta" isAbstract="true">
        <ownedComment xmi:type="uml:Comment" xmi:id="EAID_N1" body="Una cuenta &amp; su saldo" annotatedElement="EAID_C"/>
        <ownedAttribute xmi:type="uml:Property" xmi:id="EAID_A1" name="saldo" visibility="private">
          <type xmi:idref="EAJava_double"/>
          <lowerValue xmi:type="uml:LiteralInteger" xmi:id="EAID_L1" value="1"/>
          <upperValue xmi:type="uml:LiteralUnlimitedNatural" xmi:id="EAID_U1" value="1"/>
        </ownedAttribute>
        <ownedAttribute xmi:type="uml:Property" xmi:id="EAID_A2" name="titular">
          <type href="http://www.omg.org/spec/UML/20131001/PrimitiveTypes.xmi#String"/>
        </ownedAttribute>
        <ownedAttribute xmi:type="uml:Property" xmi:id="EAID_END_M" name="movimientos" aggregation="composite"
                        type="EAID_M" association="EAID_AS">
          <lowerValue xmi:type="uml:LiteralInteger" xmi:id="EAID_L2"/>
          <upperValue xmi:type="uml:LiteralUnlimitedNatural" xmi:id="EAID_U2" value="*"/>
        </ownedAttribute>
        <ownedOperation xmi:type="uml:Operation" xmi:id="EAID_O1" name="retirar">
          <ownedParameter xmi:type="uml:Parameter" xmi:id="EAID_P1" name="importe" direction="in" type="EAID_Money"/>
          <ownedParameter xmi:type="uml:Parameter" xmi:id="EAID_P2" direction="return">
            <type href="http://www.omg.org/spec/UML/20131001/PrimitiveTypes.xmi#Boolean"/>
          </ownedParameter>
        </ownedOperation>
      </packagedElement>
      <packagedElement xmi:type="uml:Class" xmi:id="EAID_A" name="Ahorro">
        <generalization xmi:type="uml:Generalization" xmi:id="EAID_G" general="EAID_C"/>
      </packagedElement>
      <packagedElement xmi:type="uml:Class" xmi:id="EAID_M" name="Movimiento"/>
      <packagedElement xmi:type="uml:DataType" xmi:id="EAID_Money" name="Money"/>
      <packagedElement xmi:type="uml:Association" xmi:id="EAID_AS" memberEnd="EAID_END_M EAID_END_C">
        <ownedEnd xmi:type="uml:Property" xmi:id="EAID_END_C" name="cuenta" type="EAID_C" association="EAID_AS"/>
      </packagedElement>
      <packagedElement xmi:type="uml:StateMachine" xmi:id="EAID_SM" name="Ciclo"/>
      <packagedElement xmi:type="uml:Signal" xmi:id="EAID_SIG" name="Alerta"/>
      <packagedElement xmi:type="uml:Dependency" xmi:id="EAID_D" client="EAID_A" supplier="EAID_NOWHERE"/>
    </packagedElement>
  </uml:Model>
  <thecustomprofile:entity base_Class="EAID_C"/>
  <xmi:Extension extender="Enterprise Architect" extenderID="6.5">
    <diagrams>
      <diagram xmi:id="EAID_DG">
        <model package="EAPK_1" localID="1" owner="EAPK_1"/>
        <properties name="Cuentas" type="Logical"/>
        <elements>
          <element geometry="Left=100;Top=50;Right=250;Bottom=150;" subject="EAID_C" seqno="1"/>
          <element geometry="Left=100;Top=250;Right=250;Bottom=330;" subject="EAID_A" seqno="2"/>
          <element geometry="SX=0;SY=0;EX=0;EY=0;" subject="EAID_G"/>
        </elements>
      </diagram>
    </diagrams>
  </xmi:Extension>
</xmi:XMI>
)XMI";

} // namespace

int main(int argc, char* argv[]) {
    std::cout << "==========================================\n";
    std::cout << " Running onlyArchitect XMI Tests (no Qt)\n";
    std::cout << "==========================================\n";

    // 1. XML reader / writer
    {
        auto r = xml::parse("<?xml version=\"1.0\"?><!DOCTYPE a [<!ELEMENT a ANY>]><!-- c --><a x='1 &lt; 2' y=\"&#x41;&#66;\">"
                            "t&amp;<b/><![CDATA[<raw>]]><ns:c ns:k=\"v\">z</ns:c></a>");
        assert(r.ok() && r.root->name == "a" && r.root->attr("x") == "1 < 2" && r.root->attr("y") == "AB");
        assert(r.root->text == "t&<raw>" && r.root->children.size() == 2);
        assert(r.root->child("c") && r.root->child("c")->attr("k") == "v" && r.root->child("ns:c")->localName() == "c");
        auto typed = xml::parse("<p xmi:type=\"uml:Property\"/>");
        assert(typed.root->attr("type").empty() && typed.root->attr("xmi:type") == "uml:Property");
        auto bad = xml::parse("<a>\n<b>\n</a>");
        assert(!bad.ok() && bad.line == 3 && !bad.error.empty());
        assert(!xml::parse("<a x=\"&nope;\"/>").ok() && !xml::parse("").ok() && !xml::parse("<a/><b/>").ok());
        std::string deep;
        for (int i = 0; i < 100000; ++i) deep += "<d>";
        for (int i = 0; i < 100000; ++i) deep += "</d>";
        assert(xml::parse(deep).ok() && "deep documents do not overflow the stack");

        xml::Writer w;
        w.open("r").attr("note", "línea 1\nlínea \"2\" <&>").open("e").text("x < y").close().open("empty").close();
        auto back = xml::parse(w.str());
        assert(back.ok() && back.root->attr("note") == "línea 1\nlínea \"2\" <&>" && back.root->child("e")->text == "x < y");

        // Security: XXE / Billion Laughs / external DTD / NUL bytes / non-XML / illegal char refs must be rejected
        assert(!xml::parse("<!DOCTYPE x [<!ENTITY xxe SYSTEM \"file:///etc/passwd\">]><x>&xxe;</x>").ok());
        assert(!xml::parse("<!DOCTYPE x SYSTEM \"http://evil.invalid/dtd\"><x/>").ok());
        assert(!xml::parse(std::string("<x>\0</x>", 8)).ok());
        assert(!xml::parse("not xml at all <x/>").ok());
        assert(!xml::parse("<x a=\"&#0;\"/>").ok());
    }
    std::cout << "  ✓ XML reader/writer: entities, CDATA, DOCTYPE, XXE/injection guards, deep nesting, escaping\n";

    // 2. A model with every construct
    fs::path src = fs::temp_directory_path() / "onlyarchitect_test_xmi_src.qea";
    fs::path dst = fs::temp_directory_path() / "onlyarchitect_test_xmi_dst.qea";
    fs::remove(src);
    fs::remove(dst);
    std::string exported;
    {
        Repository repo;
        assert(repo.CreateRepository(src.string()));
        const int root = repo.GetModels().GetAt(0)->GetPackageID();
        auto ventas = repo.CreatePackage(root, "Ventas", "Paquete de ventas");
        auto dominio = repo.CreatePackage(ventas->GetPackageID(), "Dominio");
        assert(ventas && dominio);
        const int pv = ventas->GetPackageID(), pd = dominio->GetPackageID();

        auto pedido = add(repo, pd, ObjectType::Class, "Pedido", "entity");
        pedido->SetAlias("Order");
        pedido->SetNotes("Un pedido\ncon \"varias\" líneas & <totales>");
        pedido->SetAbstract(true);
        auto id = pedido->GetAttributes().AddNew("id", "long");
        id->SetVisibility(Scope::Private);
        auto lineas = pedido->GetAttributes().AddNew("total", "Money");
        lineas->SetLowerBound("0");
        lineas->SetUpperBound("*");
        lineas->SetStatic(true);
        lineas->SetDefault("0.0");
        auto op = pedido->GetMethods().AddNew("calcular", "Money");
        op->SetParameters(editor::parse_parameters("moneda: string = \"EUR\", out redondeo: int").parameters);
        op->SetAbstract(true);
        assert(repo.SaveElement(*pedido));
        auto linea = add(repo, pd, ObjectType::Class, "Linea");
        auto money = add(repo, pd, ObjectType::DataType, "Money");
        auto pago = add(repo, pd, ObjectType::Interface, "IPago");
        auto estado = add(repo, pd, ObjectType::Enumeration, "Estado");
        estado->GetAttributes().AddNew("Abierto", "");
        estado->GetAttributes().AddNew("Cerrado", "");
        assert(repo.SaveElement(*estado));
        auto especial = add(repo, pd, ObjectType::Class, "PedidoEspecial");
        auto req = add(repo, pd, ObjectType::Requirement, "REQ-1");
        auto cliente = add(repo, pv, ObjectType::Actor, "Cliente");
        auto comprar = add(repo, pv, ObjectType::UseCase, "Comprar");
        auto pagar = add(repo, pv, ObjectType::UseCase, "Pagar");
        auto cupon = add(repo, pv, ObjectType::UseCase, "Usar cupón");
        auto nota = add(repo, pv, ObjectType::Note, "");
        nota->SetNotes("Revisar con negocio");
        assert(repo.SaveElement(*nota));
        auto l1 = add(repo, pv, ObjectType::Lifeline, "ui");
        auto l2 = add(repo, pv, ObjectType::Lifeline, "servicio");
        (void)money;

        link(repo, ConnectorType::Generalization, especial->GetElementID(), pedido->GetElementID());
        link(repo, ConnectorType::Realization, pedido->GetElementID(), pago->GetElementID());
        auto comp = link(repo, ConnectorType::Composition, linea->GetElementID(), pedido->GetElementID());
        comp->SetClientRole("lineas");
        comp->SetClientMultiplicity("1..*");
        comp->SetSupplierRole("pedido");
        comp->SetSupplierMultiplicity("1");
        assert(repo.SaveConnector(*comp));
        link(repo, ConnectorType::Association, cliente->GetElementID(), comprar->GetElementID());
        link(repo, ConnectorType::Dependency, pedido->GetElementID(), estado->GetElementID(), "use");
        link(repo, ConnectorType::Realization, pedido->GetElementID(), req->GetElementID());
        link(repo, ConnectorType::UseCaseLink, comprar->GetElementID(), pagar->GetElementID(), "include");
        link(repo, ConnectorType::UseCaseLink, cupon->GetElementID(), comprar->GetElementID(), "extend");
        link(repo, ConnectorType::NoteLink, nota->GetElementID(), comprar->GetElementID());
        Connector msg;
        msg.SetType(ConnectorType::Message);
        msg.SetName("enviar()");
        msg.SetClientID(l1->GetElementID());
        msg.SetSupplierID(l2->GetElementID());
        sequence::set_message_sort(msg, sequence::MessageSort::Asynchronous);
        sequence::set_message_y(msg, 140);
        msg.SetSequenceNo(1);
        auto m = repo.AddConnector(msg);

        auto diagram = repo.CreateDiagram(pd, "Clases", DiagramType::Logical);
        for (auto [e, x] : {std::pair{pedido, 100}, std::pair{linea, 400}}) {
            auto o = std::make_shared<DiagramObject>();
            o->SetDiagramID(diagram->GetDiagramID());
            o->SetElementID(e->GetElementID());
            o->SetLeft(x); o->SetRight(x + 170); o->SetTop(-80); o->SetBottom(-180);
            assert(repo.AddElementToDiagram(*diagram, o));
        }
        auto dl = std::make_shared<DiagramLink>();
        dl->SetDiagramID(diagram->GetDiagramID());
        dl->SetConnectorID(comp->GetConnectorID());
        assert(repo.AddConnectorToDiagram(*diagram, dl));
        auto seq = repo.CreateDiagram(pv, "Envío", DiagramType::Sequence);
        for (auto [e, x] : {std::pair{l1, 50}, std::pair{l2, 300}}) {
            auto o = std::make_shared<DiagramObject>();
            o->SetDiagramID(seq->GetDiagramID());
            o->SetElementID(e->GetElementID());
            o->SetLeft(x); o->SetRight(x + 110); o->SetTop(-50); o->SetBottom(-470);
            assert(repo.AddElementToDiagram(*seq, o));
        }
        auto ml = std::make_shared<DiagramLink>();
        ml->SetDiagramID(seq->GetDiagramID());
        ml->SetConnectorID(m->GetConnectorID());
        assert(repo.AddConnectorToDiagram(*seq, ml));

        auto result = xmi::export_package(repo, *repo.GetPackageByID(pv));
        exported = result.xml;
        assert(result.packages == 2 && result.elements == 14 && result.relationships == 10 && result.diagrams == 2);
        assert(result.warnings.empty());
        repo.CloseFile();
    }
    if (const char* dump = std::getenv("XMI_DUMP")) { std::ofstream(dump) << exported; }
    const auto types = check_integrity(exported);
    for (const char* t : {"uml:Package", "uml:Class", "uml:Interface", "uml:Enumeration", "uml:DataType", "uml:Actor",
                          "uml:UseCase", "uml:Generalization", "uml:InterfaceRealization", "uml:Realization",
                          "uml:Association", "uml:Include", "uml:Extend", "uml:Dependency", "uml:Comment",
                          "uml:Lifeline", "uml:LiteralUnlimitedNatural", "uml:OpaqueExpression", "uml:EnumerationLiteral"}) {
        if (!types.count(t)) std::cerr << "    missing " << t << "\n";
        assert(types.count(t) && t);
    }
    assert(exported.find("aggregation=\"composite\"") != std::string::npos);
    assert(exported.find("oaprofile:entity") != std::string::npos && exported.find("PrimitiveTypes.xmi#String") != std::string::npos);
    std::cout << "  ✓ Export: UML 2.5.1 metaclasses, unique ids, every reference resolves\n";

    // 3. Round trip into another repository
    {
        Repository repo;
        assert(repo.CreateRepository(dst.string()));
        const int root = repo.GetModels().GetAt(0)->GetPackageID();
        auto r = xmi::import_into(repo, root, exported);
        if (!r.ok) std::cerr << "    " << r.error << "\n";
        for (const auto& w : r.warnings) std::cerr << "    warning: " << w << "\n";
        assert(r.ok && r.warnings.empty());
        assert(r.packages == 2 && r.elements == 14 && r.relationships == 10 && r.diagrams == 2 && r.topPackageIds.size() == 1);
        auto ventas = repo.GetPackageByID(r.topPackageIds[0]);
        assert(ventas->GetName() == "Ventas" && ventas->GetNotes() == "Paquete de ventas" && ventas->GetPackages().size() == 1);

        auto pedido = byName(repo, "Pedido");
        assert(pedido->GetType() == ObjectType::Class && pedido->GetStereotype() == "entity" && pedido->GetAlias() == "Order");
        assert(pedido->GetNotes() == "Un pedido\ncon \"varias\" líneas & <totales>" && pedido->IsAbstract());
        assert(pedido->GetAttributes().size() == 2);
        auto total = pedido->GetAttributes().GetAt(1);
        assert(total->GetName() == "total" && total->GetType() == "Money" && total->GetLowerBound() == "0" &&
               total->GetUpperBound() == "*" && total->IsStatic() && total->GetDefault() == "0.0");
        assert(pedido->GetAttributes().GetAt(0)->GetVisibility() == Scope::Private && pedido->GetAttributes().GetAt(0)->GetType() == "long");
        auto calc = pedido->GetMethods().GetAt(0);
        assert(calc->GetReturnType() == "Money" && calc->IsAbstract());
        assert(editor::format_parameters(calc->GetParameters()) == "moneda: string = \"EUR\", out redondeo: int");
        assert(byName(repo, "Estado")->GetType() == ObjectType::Enumeration && byName(repo, "Estado")->GetAttributes().size() == 2);
        assert(byName(repo, "REQ-1")->GetType() == ObjectType::Requirement && "kinds without a UML metaclass survive");
        assert(byName(repo, "ui")->GetType() == ObjectType::Lifeline);

        assert(connectorBetween(repo, "PedidoEspecial", "Pedido", ConnectorType::Generalization));
        assert(connectorBetween(repo, "Pedido", "IPago", ConnectorType::Realization));
        assert(connectorBetween(repo, "Pedido", "REQ-1", ConnectorType::Realization));
        auto comp = connectorBetween(repo, "Linea", "Pedido", ConnectorType::Composition);
        assert(comp && comp->GetClientRole() == "lineas" && comp->GetClientMultiplicity() == "1..*" && comp->GetSupplierRole() == "pedido");
        assert(connectorBetween(repo, "Cliente", "Comprar", ConnectorType::Association));
        assert(connectorBetween(repo, "Pedido", "Estado", ConnectorType::Dependency)->GetStereotype() == "use");
        assert(connectorBetween(repo, "Comprar", "Pagar", ConnectorType::UseCaseLink)->GetStereotype() == "include");
        assert(connectorBetween(repo, "Usar cupón", "Comprar", ConnectorType::UseCaseLink)->GetStereotype() == "extend");
        auto msg = connectorBetween(repo, "ui", "servicio", ConnectorType::Message);
        assert(msg && msg->GetName() == "enviar()" && sequence::message_sort(*msg) == sequence::MessageSort::Asynchronous &&
               sequence::message_y(*msg) == 140 && msg->GetSequenceNo() == 1);
        assert(count(repo, "SELECT count(*) AS n FROM t_connector WHERE Connector_Type='NoteLink'") == "1");

        auto clases = count(repo, "SELECT count(*) AS n FROM t_diagramobjects o JOIN t_diagram d ON d.Diagram_ID=o.Diagram_ID "
                                  "WHERE d.Name='Clases' AND o.RectLeft=400 AND o.RectTop=-80 AND o.RectBottom=-180");
        assert(clases == "1" && "diagram geometry preserved");
        assert(count(repo, "SELECT count(*) AS n FROM t_diagramlinks") == "2");
        assert(count(repo, "SELECT count(*) AS n FROM t_diagram WHERE Diagram_Type='Sequence'") == "1");

        // Importing twice gives a second copy, not a clash
        auto again = xmi::import_into(repo, root, exported);
        assert(again.ok && count(repo, "SELECT count(*) AS n FROM t_object WHERE Name='Pedido'") == "2");
        repo.CloseFile();
    }
    std::cout << "  ✓ Round trip: packages, kinds, stereotypes, features, all relationship kinds, messages, diagrams\n";

    // 4. Foreign XMI (EA 2.1 style, no onlyArchitect extension)
    {
        Repository repo;
        assert(repo.OpenFile(dst.string()));
        const int root = repo.GetModels().GetAt(0)->GetPackageID();
        auto r = xmi::import_into(repo, root, kForeignXmi);
        assert(r.ok);
        assert(r.packages == 1 && r.elements == 5 && r.diagrams == 1);
        assert(r.warnings.size() == 2 && "uml:Signal skipped and the dangling dependency reported");
        auto cuenta = byName(repo, "Cuenta");
        assert(cuenta->GetStereotype() == "entity" && cuenta->IsAbstract() && cuenta->GetNotes() == "Una cuenta & su saldo");
        assert(cuenta->GetAttributes().size() == 2 && "the association end is not an attribute");
        assert(cuenta->GetAttributes().GetAt(0)->GetType() == "double" && cuenta->GetAttributes().GetAt(0)->GetVisibility() == Scope::Private);
        assert(cuenta->GetAttributes().GetAt(1)->GetType() == "String");
        auto retirar = cuenta->GetMethods().GetAt(0);
        assert(retirar->GetReturnType() == "Boolean" && retirar->GetParameters().at(0).type == "Money");
        assert(connectorBetween(repo, "Ahorro", "Cuenta", ConnectorType::Generalization));
        auto comp = connectorBetween(repo, "Movimiento", "Cuenta", ConnectorType::Composition);
        assert(comp && comp->GetClientRole() == "movimientos" && comp->GetClientMultiplicity() == "*" && comp->GetSupplierRole() == "cuenta");
        assert(byName(repo, "Ciclo")->GetType() == ObjectType::StateMachine);
        auto geometry = count(repo, "SELECT count(*) AS n FROM t_diagramobjects o JOIN t_diagram d ON d.Diagram_ID=o.Diagram_ID "
                                    "WHERE d.Name='Cuentas' AND o.RectTop=-50 AND o.RectBottom=-150");
        assert(geometry == "1" && "EA's positive XMI coordinates are stored as the repository expects");
        repo.CloseFile();
    }
    std::cout << "  ✓ Foreign XMI: EA type refs, primitive hrefs, class-owned association ends, profile stereotypes, EA diagrams\n";

    // 5. Errors leave the repository untouched
    {
        Repository repo;
        assert(repo.OpenFile(dst.string()));
        const int root = repo.GetModels().GetAt(0)->GetPackageID();
        const auto before = count(repo, "SELECT count(*) AS n FROM t_object");
        auto broken = xmi::import_into(repo, root, "<xmi:XMI><uml:Model><packagedElement xmi:type=\"uml:Class\" name=\"X\">");
        assert(!broken.ok && broken.error.find("línea") != std::string::npos && !broken.repositoryReloadNeeded);
        assert(!xmi::import_into(repo, root, "<foo/>").ok);
        assert(!xmi::import_into(repo, 999999, kForeignXmi).ok && "unknown target package");
        assert(count(repo, "SELECT count(*) AS n FROM t_object") == before);
        repo.CloseFile();
    }
    fs::remove(src);
    fs::remove(dst);
    std::cout << "  ✓ Malformed or foreign-but-empty documents and bad targets are refused, nothing written\n";

    // 6. Optional external full-size model (pass via argv[1] or ONLYARCHITECT_SAMPLE_QEA)
    const char* envSample = std::getenv("ONLYARCHITECT_SAMPLE_QEA");
    fs::path sample = argc > 1 ? fs::path(argv[1]) : (envSample && *envSample ? fs::path(envSample) : fs::path{});
    if (!sample.empty() && fs::exists(sample)) {
        Repository ea;
        assert(ea.OpenFile(sample.string()));
        auto model = ea.GetModels().GetAt(0);
        const auto t0 = std::chrono::steady_clock::now();
        auto r = xmi::export_package(ea, *model);
        const auto t1 = std::chrono::steady_clock::now();
        check_integrity(r.xml);
        ea.CloseFile();

        fs::path out = fs::temp_directory_path() / "onlyarchitect_test_xmi_ea.qea";
        fs::remove(out);
        Repository repo;
        assert(repo.CreateRepository(out.string()));
        const auto t2 = std::chrono::steady_clock::now();
        auto in = xmi::import_into(repo, repo.GetModels().GetAt(0)->GetPackageID(), r.xml);
        const auto t3 = std::chrono::steady_clock::now();
        assert(in.ok);
        if (!(in.packages == r.packages && in.elements == r.elements && in.relationships == r.relationships && in.diagrams == r.diagrams)) {
            std::cerr << "    exported " << r.packages << "/" << r.elements << "/" << r.relationships << "/" << r.diagrams << " imported "
                      << in.packages << "/" << in.elements << "/" << in.relationships << "/" << in.diagrams << "\n";
            for (const auto& w : r.warnings) std::cerr << "    export warning: " << w << "\n";
            for (const auto& w : in.warnings) std::cerr << "    import warning: " << w << "\n";
        }
        assert(in.packages == r.packages && in.elements == r.elements && in.relationships == r.relationships && in.diagrams == r.diagrams);
        repo.CloseFile();
        fs::remove(out);
        auto ms = [](auto a, auto b) { return std::chrono::duration_cast<std::chrono::milliseconds>(b - a).count(); };
        std::cout << "  ✓ Sample .qea: " << r.packages << " packages, " << r.elements << " elements, " << r.relationships
                  << " relationships, " << r.diagrams << " diagrams; export " << ms(t0, t1) << " ms, import " << ms(t2, t3)
                  << " ms, " << r.xml.size() / 1024 << " KiB, counts identical after re-import\n";
    }

    std::cout << "==========================================\n";
    std::cout << " ALL XMI TESTS PASSED!\n";
    std::cout << "==========================================\n";
    return 0;
}
