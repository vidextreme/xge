#include "pch.h"
#include "xgScriptEngine.h"
#include "xgMessenger.h"
#include "xgScriptMessage.h"
XG_MODULE_EXPORT void XGSendMessageToEngine(
    xg::ScriptEngine* engine,
    const char* typeName,
    const void* payload,
    int payloadSize,
    uint32_t sourceID)
{
    xg::ScriptMessage msg;
    msg.SourceID = sourceID;
    msg.TypeName = typeName;
    msg.Payload = payload;
    msg.PayloadSize = payloadSize;
    engine->GetMessenger()->SendToAll(msg);
}

XG_MODULE_EXPORT void XGSendMessageToEngine_Route(
    xg::ScriptEngine* engine,
    const char* typeName,
    const void* payload,
    uint32_t payloadSize,
    int sourceID,
    xg::Route route)
{
    xg::ScriptMessage msg;
    msg.SourceID = sourceID;
    msg.TypeName = typeName;
    msg.Payload = payload;
    msg.PayloadSize = payloadSize;

    engine->GetMessenger()->Send(msg, route);
}

XG_MODULE_EXPORT
xg::PayloadMode XGGetPayloadMode(xg::ScriptEngine* engine)
{
    return engine->GetPayloadMode();
}
