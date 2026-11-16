#pragma once
#include "sdk.h"

// ============================================================================
// StackTrace — Stack trace manual via EBP walk (x86)
//
// O EBP é capturado no caller (dentro do hook) e passado aqui. A caminhada
// do frame pointer é a forma mais barata de identificar o caller de uma API
// do jogo sem símbolos.
// ============================================================================

namespace StackTrace {

// Loga a cadeia de frames (prefixo + "módulo+offset!endereço de retorno").
// pEbp: ponteiro para o frame atual (capturado no caller).
void Log(const char* prefix, uintptr_t* pEbp);

} // namespace StackTrace
