// EN: Declaration of the HttpClient class and related types. Provides HTTP methods (GET, HEAD,
// POST, PUT, DELETE) with robust error and header handling. FR : Déclaration de la classe
// HttpClient et des types associés. Fournit les méthodes HTTP (GET, HEAD, POST, PUT, DELETE) avec
// gestion robuste des erreurs et des headers.

#pragma once

#include <map>
#include <optional>
#include <string>

namespace HttpClient {

// EN: Structure representing an HTTP request.
// FR : Structure représentant une requête HTTP.
struct HttpRequest {
    std::string method;
    std::string url;
    std::map<std::string, std::string> headers;
    std::optional<std::string> body;
};

// EN: Structure representing an HTTP response.
// FR : Structure représentant une réponse HTTP.
struct HttpResponse {
    int status = 0;
    std::map<std::string, std::string> headers;
    std::string body;
    long elapsed_ms = 0;
};

// EN: HttpClient class. Provides HTTP methods and manages timeouts.
// FR : Classe HttpClient. Fournit les méthodes HTTP et gère les timeouts.
class HttpClient {
  private:
    int connectTimeoutMs_;
    int readTimeoutMs_;

  public:
    // EN: Constructor. Sets timeouts.
    // FR : Constructeur. Définit les timeouts.
    HttpClient(int connectTimeoutMs, int readTimeoutMs);

    // EN: Performs a GET request.
    // FR : Effectue une requête GET.
    HttpResponse get(const std::string& url,
                     const std::map<std::string, std::string>& extraHeaders = {});
    // EN: Performs a HEAD request.
    // FR : Effectue une requête HEAD.
    HttpResponse head(const std::string& url,
                      const std::map<std::string, std::string>& extraHeaders = {});
    // EN: Performs a POST request.
    // FR : Effectue une requête POST.
    HttpResponse post(const std::string& url,
                      const std::map<std::string, std::string>& extraHeaders = {},
                      const std::string& body = "");
    // EN: Performs a PUT request.
    // FR : Effectue une requête PUT.
    HttpResponse put(const std::string& url,
                     const std::map<std::string, std::string>& extraHeaders = {},
                     const std::string& body = "");
    // EN: Performs a DELETE request.
    // FR : Effectue une requête DELETE.
    HttpResponse del(const std::string& url,
                     const std::map<std::string, std::string>& extraHeaders = {});
};

}  // namespace HttpClient
