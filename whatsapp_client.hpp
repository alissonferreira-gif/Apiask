#pragma once
// ============================================================
// AlissonAsk V0.6 — WhatsAppClient
// Criado e Integrado por: Álisson Ferreira Dos Santos
//
// Envia mensagens via WhatsApp Business API (Cloud API)
// e faz parse dos webhooks recebidos.
// ============================================================

#include <string>
#include <nlohmann/json.hpp>

// ── Mensagem recebida via webhook ─────────────────────────────
struct WhatsAppMessage {
    bool        valid = false;   // false se o payload não for uma msg de texto
    std::string from;            // número do remetente: "5511999998888"
    std::string text;            // conteúdo da mensagem
    std::string message_id;      // ID da mensagem (para marcar como lida)
    std::string display_name;    // nome do contato (pode ser vazio)
};

// ── Cliente WhatsApp ──────────────────────────────────────────
class WhatsAppClient {
public:
    struct Config {
        std::string phone_number_id;  // ID do número no Meta
        std::string access_token;     // Bearer token da API
        std::string api_version = "v19.0";
    };

    explicit WhatsAppClient(Config cfg);

    // Envia mensagem de texto simples
    void send_text(const std::string& to, const std::string& body);

    // Marca mensagem como lida (melhora UX no WhatsApp)
    void mark_as_read(const std::string& message_id);

    // Faz parse do corpo JSON do webhook recebido pelo httplib
    [[nodiscard]] static WhatsAppMessage parse_webhook(const std::string& body);

private:
    Config cfg_;

    // Monta a URL base: https://graph.facebook.com/{version}/{phone_id}/messages
    [[nodiscard]] std::string messages_url() const;

    // Executa POST na Graph API e lança exceção em caso de erro HTTP
    void post_json(const std::string& url, const nlohmann::json& payload);
};
