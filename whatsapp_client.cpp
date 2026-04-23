// ============================================================
// AlissonAsk V0.6 — WhatsAppClient (implementação)
// Criado e Integrado por: Álisson Ferreira Dos Santos
// ============================================================

#include "whatsapp_client.hpp"
#include <httplib.h>
#include <format>
#include <print>
#include <stdexcept>

using json = nlohmann::json;

// ── Construtor ────────────────────────────────────────────────
WhatsAppClient::WhatsAppClient(Config cfg)
    : cfg_(std::move(cfg)) {}

// ── URL base da Graph API ─────────────────────────────────────
std::string WhatsAppClient::messages_url() const {
    return std::format("https://graph.facebook.com/{}/{}/messages",
                       cfg_.api_version, cfg_.phone_number_id);
}

// ── POST genérico com Bearer token ───────────────────────────
void WhatsAppClient::post_json(const std::string& url, const json& payload) {
    // Separa host e path para o httplib
    // URL: https://graph.facebook.com/v19.0/{id}/messages
    httplib::SSLClient cli("graph.facebook.com");
    cli.set_bearer_token_auth(cfg_.access_token);
    cli.set_connection_timeout(10);
    cli.set_read_timeout(15);

    const std::string path = std::format("/{}/{}/messages",
                                          cfg_.api_version, cfg_.phone_number_id);
    const std::string body = payload.dump();

    auto res = cli.Post(path, body, "application/json");

    if (!res) {
        throw std::runtime_error("[WhatsApp] Falha de conexão com a Graph API");
    }
    if (res->status < 200 || res->status >= 300) {
        throw std::runtime_error(std::format(
            "[WhatsApp] Erro HTTP {}: {}", res->status, res->body));
    }
}

// ── Envia texto simples ───────────────────────────────────────
void WhatsAppClient::send_text(const std::string& to, const std::string& body) {
    json payload = {
        {"messaging_product", "whatsapp"},
        {"recipient_type",    "individual"},
        {"to",                to},
        {"type",              "text"},
        {"text",              {{"preview_url", false}, {"body", body}}}
    };

    try {
        post_json(messages_url(), payload);
        std::println("[WhatsApp] Mensagem enviada para {}", to);
    } catch (const std::exception& e) {
        std::println(stderr, "[WhatsApp] Erro ao enviar: {}", e.what());
        throw;
    }
}

// ── Marca mensagem como lida ──────────────────────────────────
void WhatsAppClient::mark_as_read(const std::string& message_id) {
    json payload = {
        {"messaging_product", "whatsapp"},
        {"status",            "read"},
        {"message_id",        message_id}
    };

    try {
        post_json(messages_url(), payload);
    } catch (const std::exception& e) {
        // Não é crítico — apenas loga
        std::println(stderr, "[WhatsApp] Aviso: não foi possível marcar como lido: {}", e.what());
    }
}

// ── Parse do webhook ──────────────────────────────────────────
WhatsAppMessage WhatsAppClient::parse_webhook(const std::string& body) {
    WhatsAppMessage wm;

    try {
        auto j = json::parse(body);

        // Estrutura: entry[0].changes[0].value.messages[0]
        auto& entry   = j.at("entry").at(0);
        auto& change  = entry.at("changes").at(0);
        auto& value   = change.at("value");

        // Ignora status updates (delivered, read, etc.)
        if (!value.contains("messages")) return wm;

        auto& msg = value.at("messages").at(0);

        // Só processa mensagens de texto por enquanto
        if (msg.value("type", "") != "text") return wm;

        wm.valid      = true;
        wm.from       = msg.at("from").get<std::string>();
        wm.message_id = msg.at("id").get<std::string>();
        wm.text       = msg.at("text").at("body").get<std::string>();

        // Nome do contato (opcional)
        if (value.contains("contacts") && !value.at("contacts").empty()) {
            wm.display_name = value.at("contacts").at(0)
                                   .value("profile", json{})
                                   .value("name", "");
        }

    } catch (const std::exception& e) {
        std::println(stderr, "[WhatsApp] Erro ao fazer parse do webhook: {}", e.what());
        wm.valid = false;
    }

    return wm;
}
