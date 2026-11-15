#pragma once
// ============================================================================
// sdk.h — Precompiled header / include guard para evitar conflitos
// winsock.h vs winsock2.h e garantir WIN32_LEAN_AND_MEAN.
//
// ESTE ARQUIVO DEVE SER O PRIMEIRO #include EM TODOS OS .cpp
// E EM TODOS OS .h QUE PRECISAM DE WINDOWS API.
// ============================================================================
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>