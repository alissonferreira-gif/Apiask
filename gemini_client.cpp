// ============================================================
//  AlissonAsk V0.6 — GeminiClient Implementation
//  Criado e Integrado por: Álisson Ferreira Dos Santos
//
//  Correções V0.6:
//  - Migrado de api_key_ para ApiKeyManager (rodízio de keys)
//  - Cache de respostas integrado
//  - Rotação automática de key ao atingir rate limit
// ============================================================

#include "gemini_client.hpp"

#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include <sstream>
#include <stdexcept>
#include <format>
#include <print>

using json = nlohmann::json;

// ── cURL write callback ───────────────────────────────────────

static size_t write_cb(char* ptr, size_t sz, size_t nmemb, void* udata) {
    static_cast<std::string*>(udata)->append(ptr, sz * nmemb);
    return sz * nmemb;
}

// ── Construtores / Destrutor ──────────────────────────────────

GeminiClient::GeminiClient(std::vector<std::string> api_keys, Config cfg)
    : key_mgr_(std::move(api_keys))
    , cache_(cfg.cache_ttl_min)
    , cfg_(std::move(cfg))
{
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl_ = curl_easy_init();
    if (!curl_) throw std::runtime_error("Falha ao inicializar libcurl");
}

GeminiClient::GeminiClient(std::string api_key, Config cfg)
    : GeminiClient(std::vector<std::string>{std::move(api_key)}, std::move(cfg))
{}

GeminiClient::~GeminiClient() {
    if (curl_) curl_easy_cleanup(static_cast<CURL*>(curl_));
    curl_global_cleanup();
}

// ── Monta payload JSON para a API do Gemini ──────────────────

std::string GeminiClient::build_payload(const std::vector<Message>& history) const {
    json body;

    if (!cfg_.system_prompt.empty()) {
        body["system_instruction"]["parts"][0]["text"] = cfg_.system_prompt;
    }

    json contents = json::array();
    contents.push_back({
        { "role",  "user"  },
        { "parts", json::array({ { { "text", "[início da conversa]" } } }) }
    });
    contents.push_back({
        { "role",  "model" },
        { "parts", json::array({ { { "text", "Entendido!" } } }) }
    });

    for (const auto& m : history) {
        contents.push_back({
            { "role",  m.role == "assistant" ? "model" : "user" },
            { "parts", json::array({ { { "text", m.content } } }) }
        });
    }

    body["contents"] = contents;
    body["generationConfig"] = {
        { "maxOutputTokens", cfg_.max_tokens  },
        { "temperature",     cfg_.temperature },
        { "stopSequences",   json::array()    },
    };

    return body.dump();
}

// ── HTTP POST via libcurl ─────────────────────────────────────

std::string GeminiClient::http_post(const std::string& url, const std::string& body) {
    CURL* curl = static_cast<CURL*>(curl_);
    std::string response;
    long http_code = 0;

    curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL,            url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER,     headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS,     body.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,  write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,      &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,        static_cast<long>(cfg_.timeout_sec));
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_slist_free_all(headers);

    if (res != CURLE_OK)
        throw GeminiException(std::format("Erro de rede: {}", curl_easy_strerror(res)));

    if (http_code == 429) throw RateLimitException("Rate limit atingido", 429);
    if (http_code == 400) throw InvalidKeyException("API key inválida ou requisição malformada", 400);
    if (http_code >= 400)
        throw GeminiException(std::format("Erro HTTP {}", http_code), static_cast<int32_t>(http_code));

    return response;
}

// ── Parseia resposta JSON ─────────────────────────────────────

ChatResponse GeminiClient::parse_response(const std::string& raw) const {
    json j;
    try {
        j = json::parse(raw);
    } catch (const json::parse_error& e) {
        throw GeminiException(std::format("Resposta JSON inválida do Gemini: {}", e.what()));
    }

    if (!j.is_object()) {
        throw GeminiException("Resposta Gemini inválida: payload raiz não é um objeto JSON");
    }

    if (j.contains("error") && j["error"].is_object()) {
        const auto& err = j["error"];
        const int32_t code = err.value("code", 0);
        const std::string msg = err.value("message", "erro desconhecido");
        throw GeminiException(std::format("Erro retornado pela API Gemini: {}", msg), code);
    }

    const auto candidates_it = j.find("candidates");
    if (candidates_it == j.end() || !candidates_it->is_array() || candidates_it->empty()) {
        throw GeminiException("Resposta Gemini inválida: campo 'candidates' ausente, não-array ou vazio");
    }

    const auto& candidate = (*candidates_it)[0];
    if (!candidate.is_object()) {
        throw GeminiException("Resposta Gemini inválida: 'candidates[0]' não é objeto");
    }

    std::string finish = "STOP";
    const auto finish_it = candidate.find("finishReason");
    if (finish_it != candidate.end()) {
        if (!finish_it->is_string()) {
            throw GeminiException("Resposta Gemini inválida: 'finishReason' presente com tipo inesperado");
        }
        finish = finish_it->get<std::string>();
    }
    if (finish == "SAFETY")
        throw SafetyFilterException("Resposta bloqueada pelo filtro de segurança do Gemini");

    const auto content_it = candidate.find("content");
    if (content_it == candidate.end() || !content_it->is_object()) {
        throw GeminiException("Resposta Gemini inválida: campo 'content' ausente ou inválido em 'candidates[0]'");
    }

    const auto parts_it = content_it->find("parts");
    if (parts_it == content_it->end() || !parts_it->is_array() || parts_it->empty()) {
        throw GeminiException("Resposta Gemini inválida: campo 'content.parts' ausente, não-array ou vazio");
    }

    const auto& first_part = (*parts_it)[0];
    if (!first_part.is_object()) {
        throw GeminiException("Resposta Gemini inválida: 'content.parts[0]' não é objeto");
    }

    const auto text_it = first_part.find("text");
    if (text_it == first_part.end() || !text_it->is_string()) {
        throw GeminiException("Resposta Gemini inválida: campo 'content.parts[0].text' ausente ou inválido");
    }

    ChatResponse resp;
    resp.content       = text_it->get<std::string>();
    resp.finish_reason = finish;

    const auto usage_it = j.find("usageMetadata");
    if (usage_it != j.end() && usage_it->is_object()) {
        const auto prompt_it = usage_it->find("promptTokenCount");
        if (prompt_it != usage_it->end() && prompt_it->is_number_integer()) {
            resp.input_tokens = prompt_it->get<int_fast32_t>();
        }

        const auto output_it = usage_it->find("candidatesTokenCount");
        if (output_it != usage_it->end() && output_it->is_number_integer()) {
            resp.output_tokens = output_it->get<int_fast32_t>();
        }
    }

    return resp;
}

// ── Extrai última mensagem do usuário (chave de cache) ────────

std::string GeminiClient::last_user_message(const std::vector<Message>& history) {
    for (auto it = history.rbegin(); it != history.rend(); ++it) {
        if (it->role == "user") return it->content;
    }
    return {};
}

// ── chat() — com cache e rodízio automático de keys ──────────

ChatResponse GeminiClient::chat(const std::vector<Message>& history) {
    // Tenta servir do cache primeiro
    const std::string cache_key = last_user_message(history);
    if (!cache_key.empty()) {
        auto cached = cache_.get(cache_key);
        if (!cached.empty()) {
            ChatResponse resp;
            resp.content    = std::move(cached);
            resp.from_cache = true;
            return resp;
        }
    }

    // Tenta até esgotar todas as keys disponíveis
    const size_t total_keys = key_mgr_.total();
    for (size_t attempt = 0; attempt < total_keys; ++attempt) {
        try {
            const std::string url = std::format(
                "https://generativelanguage.googleapis.com/v1beta/models/{}:generateContent?key={}",
                cfg_.model, key_mgr_.current()
            );

            std::string payload = build_payload(history);
            std::string raw     = http_post(url, payload);
            ChatResponse resp   = parse_response(raw);

            if (!cache_key.empty())
                cache_.set(cache_key, resp.content);

            return resp;

        } catch (const RateLimitException&) {
            std::println(stderr, "[GeminiClient] Rate limit na key {}. Rotacionando...",
                         key_mgr_.current_index());
            key_mgr_.rotate();
            if (attempt + 1 == total_keys) throw;
        }
    }

    throw RateLimitException("Todas as API keys atingiram rate limit", 429);
}
