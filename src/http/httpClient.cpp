// EN: Implementation of the HttpClient class. Provides robust HTTP methods (GET, HEAD, POST, PUT,
// DELETE) with error and header handling. FR : Implémentation de la classe HttpClient. Fournit des
// méthodes HTTP robustes (GET, HEAD, POST, PUT, DELETE) avec gestion des erreurs et des headers.

#include "../include/http/httpClient.hpp"

#include <chrono>
#include <stdexcept>

#include <curl/curl.h>

namespace HttpClient {

// EN: Callback for writing response body.
// FR : Callback pour écrire le corps de la réponse.
size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    auto* body = static_cast<std::string*>(userp);
    size_t total = size * nmemb;
    body->append((char*)contents, total);
    return total;
}

// EN: Callback for parsing response headers.
// FR : Callback pour parser les headers de la réponse.
size_t header_callback(char* buffer, size_t size, size_t nitems, void* userdata) {
    size_t total = size * nitems;
    auto* headers = static_cast<std::map<std::string, std::string>*>(userdata);
    std::string line(buffer, total);
    auto pos = line.find(":");
    if (pos != std::string::npos) {
        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);
        // Trim whitespace
        key.erase(key.find_last_not_of(" \r\n") + 1);
        value.erase(0, value.find_first_not_of(" \r\n"));
        value.erase(value.find_last_not_of(" \r\n") + 1);
        headers->insert({key, value});
    }
    return total;
}

// EN: Constructor. Initializes timeouts and CURL.
// FR : Constructeur. Initialise les timeouts et CURL.
HttpClient::HttpClient(int connectTimeoutMs, int readTimeoutMs)
    : connectTimeoutMs_(connectTimeoutMs), readTimeoutMs_(readTimeoutMs) {
    curl_global_init(CURL_GLOBAL_ALL);
}

// EN: Performs a GET request.
// FR : Effectue une requête GET.
HttpResponse HttpClient::get(const std::string& url,
                             const std::map<std::string, std::string>& extraHeaders) {
    CURL* curl = curl_easy_init();
    if (!curl) throw std::runtime_error("CURL init failed");

    HttpResponse resp;
    std::string body;
    std::map<std::string, std::string> responseHeaders;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, connectTimeoutMs_);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, readTimeoutMs_);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &responseHeaders);

    struct curl_slist* chunk = nullptr;
    for (auto& [k, v] : extraHeaders) {
        std::string line = k + ": " + v;
        chunk = curl_slist_append(chunk, line.c_str());
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);

    char errorBuffer[CURL_ERROR_SIZE] = {0};
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);

    auto start = std::chrono::steady_clock::now();
    CURLcode res = curl_easy_perform(curl);
    auto end = std::chrono::steady_clock::now();

    resp.elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    resp.body = body;
    resp.headers = responseHeaders;

    if (res == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &resp.status);
    } else {
        std::string errMsg = errorBuffer[0] ? errorBuffer : curl_easy_strerror(res);
        throw std::runtime_error(errMsg);
    }

    if (chunk) curl_slist_free_all(chunk);
    curl_easy_cleanup(curl);

    return resp;
}

// EN: Performs a HEAD request.
// FR : Effectue une requête HEAD.
HttpResponse HttpClient::head(const std::string& url,
                              const std::map<std::string, std::string>& extraHeaders) {
    CURL* curl = curl_easy_init();
    if (!curl) throw std::runtime_error("CURL init failed");

    HttpResponse resp;
    std::map<std::string, std::string> responseHeaders;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, connectTimeoutMs_);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, readTimeoutMs_);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &responseHeaders);

    struct curl_slist* chunk = nullptr;
    for (auto& [k, v] : extraHeaders) {
        std::string line = k + ": " + v;
        chunk = curl_slist_append(chunk, line.c_str());
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);

    char errorBuffer[CURL_ERROR_SIZE] = {0};
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);

    auto start = std::chrono::steady_clock::now();
    CURLcode res = curl_easy_perform(curl);
    auto end = std::chrono::steady_clock::now();

    resp.elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    resp.headers = responseHeaders;

    if (res == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &resp.status);
    } else {
        std::string errMsg = errorBuffer[0] ? errorBuffer : curl_easy_strerror(res);
        throw std::runtime_error(errMsg);
    }

    if (chunk) curl_slist_free_all(chunk);
    curl_easy_cleanup(curl);

    return resp;
}

// EN: Performs a POST request.
// FR : Effectue une requête POST.
HttpResponse HttpClient::post(const std::string& url,
                              const std::map<std::string, std::string>& extraHeaders,
                              const std::string& body) {
    CURL* curl = curl_easy_init();
    if (!curl) throw std::runtime_error("CURL init failed");
    HttpResponse resp;
    std::string responseBody;
    std::map<std::string, std::string> responseHeaders;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, body.size());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, connectTimeoutMs_);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, readTimeoutMs_);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &responseHeaders);
    struct curl_slist* chunk = nullptr;
    for (auto& [k, v] : extraHeaders) {
        std::string line = k + ": " + v;
        chunk = curl_slist_append(chunk, line.c_str());
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
    char errorBuffer[CURL_ERROR_SIZE] = {0};
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);
    auto start = std::chrono::steady_clock::now();
    CURLcode res = curl_easy_perform(curl);
    auto end = std::chrono::steady_clock::now();
    resp.elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    resp.body = responseBody;
    resp.headers = responseHeaders;
    if (res == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &resp.status);
    } else {
        std::string errMsg = errorBuffer[0] ? errorBuffer : curl_easy_strerror(res);
        throw std::runtime_error(errMsg);
    }
    if (chunk) curl_slist_free_all(chunk);
    curl_easy_cleanup(curl);
    return resp;
}

// EN: Performs a PUT request.
// FR : Effectue une requête PUT.
HttpResponse HttpClient::put(const std::string& url,
                             const std::map<std::string, std::string>& extraHeaders,
                             const std::string& body) {
    CURL* curl = curl_easy_init();
    if (!curl) throw std::runtime_error("CURL init failed");
    HttpResponse resp;
    std::string responseBody;
    std::map<std::string, std::string> responseHeaders;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, body.size());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, connectTimeoutMs_);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, readTimeoutMs_);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &responseHeaders);
    struct curl_slist* chunk = nullptr;
    for (auto& [k, v] : extraHeaders) {
        std::string line = k + ": " + v;
        chunk = curl_slist_append(chunk, line.c_str());
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
    char errorBuffer[CURL_ERROR_SIZE] = {0};
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);
    auto start = std::chrono::steady_clock::now();
    CURLcode res = curl_easy_perform(curl);
    auto end = std::chrono::steady_clock::now();
    resp.elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    resp.body = responseBody;
    resp.headers = responseHeaders;
    if (res == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &resp.status);
    } else {
        std::string errMsg = errorBuffer[0] ? errorBuffer : curl_easy_strerror(res);
        throw std::runtime_error(errMsg);
    }
    if (chunk) curl_slist_free_all(chunk);
    curl_easy_cleanup(curl);
    return resp;
}

// EN: Performs a DELETE request.
// FR : Effectue une requête DELETE.
HttpResponse HttpClient::del(const std::string& url,
                             const std::map<std::string, std::string>& extraHeaders) {
    CURL* curl = curl_easy_init();
    if (!curl) throw std::runtime_error("CURL init failed");
    HttpResponse resp;
    std::string responseBody;
    std::map<std::string, std::string> responseHeaders;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, connectTimeoutMs_);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, readTimeoutMs_);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &responseHeaders);
    struct curl_slist* chunk = nullptr;
    for (auto& [k, v] : extraHeaders) {
        std::string line = k + ": " + v;
        chunk = curl_slist_append(chunk, line.c_str());
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
    char errorBuffer[CURL_ERROR_SIZE] = {0};
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);
    auto start = std::chrono::steady_clock::now();
    CURLcode res = curl_easy_perform(curl);
    auto end = std::chrono::steady_clock::now();
    resp.elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    resp.body = responseBody;
    resp.headers = responseHeaders;
    if (res == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &resp.status);
    } else {
        std::string errMsg = errorBuffer[0] ? errorBuffer : curl_easy_strerror(res);
        throw std::runtime_error(errMsg);
    }
    if (chunk) curl_slist_free_all(chunk);
    curl_easy_cleanup(curl);
    return resp;
}

}  // namespace HttpClient
