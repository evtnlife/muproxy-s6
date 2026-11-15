#pragma once
#include <cstdint>

// ============================================================================
// MessageRegistry — Sistema de mensagens tipadas via macro de auto-registro
//
// Cada módulo registra suas mensagens com REGISTER_MESSAGE(TAG, Handler).
// O dispatch é O(n) com n pequeno (~10 tipos) — mais que suficiente.
//
// Formato na rede: "TAG|payload"
//   Ex: "ENTITY|5|4660|2|10|192.5|81.5|0.0|3.5|Spider"
//   Ex: "ATTACK_START|delay=500|range=8|type=0|hitcount=3"
// ============================================================================

namespace MessageRegistry {

// Callback: recebe o payload (string após o primeiro '|')
typedef void (*MessageHandler)(const char* payload);

// Entrada do registry
struct Entry {
    const char* tag;
    MessageHandler handler;
};

// Registra um handler para um tag
void Register(const char* tag, MessageHandler handler);

// Faz o dispatch de uma linha: extrai tag, procura no registry, chama handler
void Dispatch(const char* line, int len);

// Inicializa o registry
void Initialize();
void Shutdown();

} // namespace MessageRegistry

// ============================================================================
// Macro de auto-registro — coloque no .cpp junto com a função handler
//
// Uso:
//   void HandleEntity(const char* payload) { ... }
//   REGISTER_MESSAGE(ENTITY, HandleEntity);
// ============================================================================
#define REGISTER_MESSAGE(TAG, HandlerFunc) \
    static bool _reg_##TAG = []() { \
        MessageRegistry::Register(#TAG, HandlerFunc); \
        return true; \
    }();
