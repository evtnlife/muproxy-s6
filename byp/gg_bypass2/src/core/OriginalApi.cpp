// OriginalApi.cpp — Ponteiros das APIs originais do ws2_32 (trampolines)
#include "OriginalApi.h"

namespace OriginalApi {

static SendFn g_Send = nullptr;
static ConnectFn g_Connect = nullptr;

void Set(SendFn send, ConnectFn connect)
{
    g_Send = send;
    g_Connect = connect;
}

SendFn GetSend()
{
    return g_Send;
}

ConnectFn GetConnect()
{
    return g_Connect;
}

} // namespace OriginalApi
