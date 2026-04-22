// ============================================================
//  AlissonAsk V0.5 — ChatbotEngine Implementation
//  Criado e Integrado por: Álisson Ferreira Dos Santos
//  Arquivo: src/chatbot_engine.cpp
// ============================================================

#include "chatbot_engine.hpp"
#include <format>
#include <chrono>

// Unix timestamp atual
static int64_t now_unix() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

ChatbotEngine::ChatbotEngine(
    GeminiClient&        gemini,
    ConversationManager& conv,
    IDatabase&           db)
    : gemini_(gemini), conv_(conv), db_(db) {}

// ── Processa mensagem de texto ─────────────────────────────────

EngineResult ChatbotEngine::handle_message(
    const std::string& phone_id,
    const std::string& message)
{
    // 1. Garante usuário no banco
    auto user = db_.get_or_create_user(phone_id);

    // 2. Adiciona pontos pela mensagem (int_fast32_t → soma rápida)
    uint32_t total = db_.add_points(user.id, 5u, "message");

    // 3. IA processa a mensagem
    ChatResponse ai_resp = conv_.reply(phone_id, message);

    // 4. Registra doação se detectada (aqui você pode expandir com NLP)
    // Exemplo simplificado: palavra-chave "doei"
    if (message.find("doei") != std::string::npos) {
        Donation d;
        d.user_id       = user.id;
        d.campaign_id   = 0;      // 0 = não especificada
        d.type          = "physical";
        d.points_earned = 50u;
        d.registered_at = now_unix();
        db_.register_donation(d);
        total = db_.add_points(user.id, 50u, "donation_physical");
    }

    // 5. Monta resultado
    EngineResult result;
    result.reply         = ai_resp.content;
    result.points_earned = 5u;
    result.total_points  = total;
    result.level         = points_to_level(total);

    // 6. Verifica conquistas
    user.points = total;
    check_achievements(user, result);

    return result;
}

// ── Processa doação online ─────────────────────────────────────

EngineResult ChatbotEngine::handle_donation(
    const std::string& phone_id,
    const std::string& campaign_id)
{
    auto user = db_.get_or_create_user(phone_id);

    Donation d;
    d.user_id       = user.id;
    d.campaign_id   = 0;        // equipe de BD faz lookup por slug
    d.type          = "online";
    d.points_earned = 100u;
    d.registered_at = now_unix();
    db_.register_donation(d);

    uint32_t total = db_.add_points(user.id, 100u, "donation_online");

    EngineResult result;
    result.reply         = std::format(
        "💚 Doação registrada para a campanha {}!\n\n"
        "Você ganhou +100 pontos. Total: {} pts — Nível: {}.\n\n"
        "Juntos estamos transformando vidas! Posso ajudar com mais alguma coisa? 🤝",
        campaign_id, total, points_to_level(total));
    result.points_earned = 100u;
    result.total_points  = total;
    result.level         = points_to_level(total);

    user.points = total;
    check_achievements(user, result);
    return result;
}

// ── Processa cadastro de voluntário ───────────────────────────

EngineResult ChatbotEngine::handle_volunteer(
    const std::string& phone_id,
    const std::string& full_name,
    const std::string& email,
    const std::string& available_days)
{
    auto user = db_.get_or_create_user(phone_id);

    VolunteerRegistration reg;
    reg.user_id        = user.id;
    reg.full_name      = full_name;
    reg.email          = email;
    reg.available_days = available_days;
    reg.registered_at  = now_unix();
    db_.register_volunteer(reg);

    uint32_t total = db_.add_points(user.id, 200u, "volunteer");
    db_.unlock_achievement(user.id, "volunteer");

    EngineResult result;
    result.reply = std::format(
        "🙌 Bem-vindo à equipe, {}!\n\n"
        "Seu cadastro foi recebido. Entraremos em contato pelo e-mail {}.\n"
        "+200 pontos adicionados! Total: {} pts — Nível: {}.",
        full_name, email, total, points_to_level(total));
    result.points_earned          = 200u;
    result.total_points           = total;
    result.level                  = points_to_level(total);
    result.achievement_unlocked   = true;
    result.achievement_name       = "🙌 Voluntário";

    return result;
}

// ── Verifica e desbloqueia conquistas ─────────────────────────

void ChatbotEngine::check_achievements(const UserProfile& user, EngineResult& out) {
    // Primeira doação
    if (user.donations == 1)
        if (db_.unlock_achievement(user.id, "first_don")) {
            out.achievement_unlocked = true;
            out.achievement_name     = "💝 Primeiro Coração";
        }
    // 5 doações
    if (user.donations >= 5)
        db_.unlock_achievement(user.id, "don_5");
    // Nível Deus
    if (user.points >= 2500)
        if (db_.unlock_achievement(user.id, "god_level")) {
            out.achievement_unlocked = true;
            out.achievement_name     = "⚡ Ascensão Divina";
        }
}
