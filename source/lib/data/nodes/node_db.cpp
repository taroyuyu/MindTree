#include "node_db.h"
#include "data/nodes/data_node.h"

using namespace MindTree;

std::vector<std::unique_ptr<AbstractNodeDecorator>> NodeDataBase::nodeFactories;
std::unordered_map<std::string, std::vector<AbstractNodeDecorator*>> NodeDataBase::s_converters;

AbstractNodeDecorator::AbstractNodeDecorator(std::string type, std::string label)
    : type(type), label(label)
{
    NodeType::registerType(type);
}

void AbstractNodeDecorator::setLabel(std::string l)
{
    label=l;
}

std::string AbstractNodeDecorator::getLabel()
{
    return label;
}

void AbstractNodeDecorator::setType(std::string t)
{
    type = t;
}

NodePtr AbstractNodeDecorator::operator()(bool raw)
{
    auto node = createNode(raw);

    createChildNodes(node);
    return node;
}

void AbstractNodeDecorator::createChildNodes(NodePtr node)
{
    for (DinSocket *socket : node->getInSockets()) {
        for (auto *factory : NodeDataBase::getConverters(socket->getType())) {
            socket->setConverter((*factory)(false));
        }
    }
}

std::string AbstractNodeDecorator::getType()
{
    return type;
}

BuildInDecorator::BuildInDecorator(std::string type, std::string label, std::function<NodePtr(bool)> func)
    : AbstractNodeDecorator(type, label), func(func)
{
}

NodePtr BuildInDecorator::createNode(bool raw)
{
    return func(raw);
}

void NodeDataBase::registerNodeType(std::unique_ptr<AbstractNodeDecorator> &&factory)
{
    NodePtr prototype = (*factory)(false);
    if(prototype->getOutSockets().size() == 1) {
        std::string type_string = prototype->getOutSockets()[0]->getType().toStr();
        if(s_converters.find(type_string) == end(s_converters))
        s_converters[type_string] = std::vector<AbstractNodeDecorator*>();
        s_converters[type_string].push_back(factory.get());
    }

    nodeFactories.push_back(std::move(factory));
}

NodePtr NodeDataBase::createNode(std::string name)
{
    for(const auto &fac : nodeFactories)
        if(fac->getLabel() == name)
            return (*fac)(false);

    std::cout << "node label \"" << name << "\" not found" << std::endl;
    return nullptr;
}

NodePtr NodeDataBase::createNodeByType(const NodeType &t)
{
    for(const auto &fac : nodeFactories)
        if(t == fac->getType())
            return (*fac)(true);

    std::cout << "node type \"" << t.toStr() << "\" not found" << std::endl;
    return nullptr;
}

std::vector<AbstractNodeDecorator*> NodeDataBase::getConverters(DataType t)
{
    if(s_converters.find(t.toStr()) == end(s_converters))
        return std::vector<AbstractNodeDecorator*>();
    return s_converters[t.toStr()];
}

std::vector<AbstractNodeDecorator*> NodeDataBase::getFactories()
{
    std::vector<AbstractNodeDecorator*> factories;
    for(const auto &fac : nodeFactories) {
        factories.push_back(fac.get());
    }
    return factories;
}
