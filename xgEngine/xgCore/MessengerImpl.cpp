#include "pch.h"

#include "MessengerImpl.h"
#include "ScriptTree.h"
#include "xgScriptModule.h"

namespace xg
{
    MessengerImpl::MessengerImpl(ScriptTree* tree)
        : _tree(tree)
    {}

    void MessengerImpl::Send(const ScriptMessage& msg, const Route& route)
    {
        switch (route.Type)
        {
        case RouteType::Direct:    RouteDirect(msg, route);    break;
        case RouteType::Children:  RouteChildren(msg, route);  break;
        case RouteType::Parents:   RouteParents(msg, route);   break;
        case RouteType::Siblings:  RouteSiblings(msg, route);  break;
        case RouteType::Filtered:  RouteFiltered(msg, route);  break;
        }
    }

    void MessengerImpl::SendToAll(const ScriptMessage& msg)
    {
        _tree->ForEachModule([&](ScriptModule* m) {
            m->OnMessage(msg);
            });
    }

    // -------------------------------------------------------------------------
    // Direct
    // -------------------------------------------------------------------------
    void MessengerImpl::RouteDirect(const ScriptMessage& msg, const Route& route)
    {
        ScriptModule* m = _tree->FindModuleById(route.Id);
        if (m)
            m->OnMessage(msg);
    }

    // -------------------------------------------------------------------------
    // Children (subtree)
    // -------------------------------------------------------------------------
    void MessengerImpl::RouteChildren(const ScriptMessage& msg, const Route& route)
    {
        ScriptNode* node = _tree->FindModuleById(route.Id)
            ? _tree->FindNode(_tree->FindModuleById(route.Id))
            : nullptr;

        if (!node)
            return;

        _tree->ForEachModuleRecursive(node, [&](ScriptModule* m) {
            m->OnMessage(msg);
            });
    }

    // -------------------------------------------------------------------------
    // Parents (bubble upward)
    // -------------------------------------------------------------------------
    void MessengerImpl::RouteParents(const ScriptMessage& msg, const Route& route)
    {
        ScriptNode* node = _tree->FindNode(_tree->FindModuleById(route.Id));
        if (!node)
            return;

        int depth = 0;

        while (node->GetParent() &&
            (route.DepthLimit < 0 || depth < route.DepthLimit))
        {
            node = node->GetParent();
            node->GetModule()->OnMessage(msg);
            depth++;
        }
    }

    // -------------------------------------------------------------------------
    // Siblings (strict lateral)
    // -------------------------------------------------------------------------
    void MessengerImpl::RouteSiblings(const ScriptMessage& msg, const Route& route)
    {
        ScriptModule* module = _tree->FindModuleById(route.Id);
        if (!module)
            return;

        ScriptNode* node = _tree->FindNode(module);
        if (!node)
            return;

        _tree->ForEachSibling(node, [&](ScriptModule* m) {
            m->OnMessage(msg);
            });
    }

    // -------------------------------------------------------------------------
    // Filtered
    // -------------------------------------------------------------------------
    void MessengerImpl::RouteFiltered(const ScriptMessage& msg, const Route& route)
    {
        _tree->ForEachModule([&](ScriptModule* m) {
            if (route.Predicate && route.Predicate(m))
                m->OnMessage(msg);
            });
    }
}
