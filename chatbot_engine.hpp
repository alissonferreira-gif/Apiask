#pragma once
// ============================================================
//  AlissonAsk V0.5 — ChatbotEngine
//  Criado e Integrado por: Álisson Ferreira Dos Santos
//  Arquivo: include/chatbot_engine.hpp
//
//  Orquestra: recebe mensagem → adiciona pontos → IA responde
//  → registra no banco → retorna resposta pronta para envio.
// ============================================================

#include "gemini_client.hpp"
#include "conversation_manager.hpp"
#include "idatabase.hpp"

#include <string>
#include <cstdint>
#include <functional>

// Resultado de cada interação
struct EngineResult {
    std::string  reply;           // texto a enviar ao usuário
    uint32_t     points_earned;   // pontos ganhos nesta interação
    uint32_t     total_points;    // total acumulado do usuário
    std::string  level;           // nível atual: "⚡ Deus", etc.
    bool         achievement_unlocked = false;
    std::string  achievement_name;
};

class ChatbotEngine {
public:
    explicit ChatbotEngine(
        GeminiClient&       gemini,
        ConversationManager& conv,
        IDatabase&          db
    );

    // Processa mensagem de texto vinda do WhatsApp
    [[nodiscard]] EngineResult handle_message(
        const std::string& phone_id,    // "+5511999998888"
        const std::string& message
    );

    // Processa intenção de doação online
    [[nodiscard]] EngineResult handle_donation(
        const std::string& phone_id,
        const std::string& campaign_id  // "fome_zero" etc.
    );

    // Processa cadastro de voluntário
    [[nodiscard]] EngineResult handle_volunteer(
        const std::string& phone_id,
        const std::string& full_name,
        const std::string& email,
        const std::string& available_days
    );

private:
    GeminiClient&        gemini_;
    ConversationManager& conv_;
    IDatabase&           db_;

    // Verifica e desbloqueia conquistas após cada ação
    void check_achievements(const UserProfile& user, EngineResult& out);
};
