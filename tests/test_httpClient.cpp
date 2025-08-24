#include <cassert>
#include <iostream>

#include "../src/include/http/httpClient.hpp"

int main() {
    HttpClient::HttpClient client(2000, 2000);

    // Test HEAD request
    try {
        HttpClient::HttpResponse resp = client.head("https://httpbin.org/get");
        std::cout << "HEAD status: " << resp.status << ", elapsed: " << resp.elapsed_ms << "ms\n";
        assert(resp.status >= 200 && resp.status < 500);
        assert(!resp.headers.empty());
    } catch (const std::exception& e) {
        std::cerr << "HEAD request failed: " << e.what() << std::endl;
        return 1;
    }

    // Test GET request
    try {
        HttpClient::HttpResponse resp = client.get("https://httpbin.org/get");
        std::cout << "GET status: " << resp.status << ", elapsed: " << resp.elapsed_ms << "ms\n";
        assert(resp.status == 200);
        assert(!resp.body.empty());
        assert(!resp.headers.empty());
    } catch (const std::exception& e) {
        std::cerr << "GET request failed: " << e.what() << std::endl;
        return 1;
    }

    // Test POST request
    try {
        std::map<std::string, std::string> headers = {{"Content-Type", "application/json"}};
        std::string postData = "{\"test\":\"value\"}";
        HttpClient::HttpResponse resp = client.post("https://httpbin.org/post", headers, postData);
        std::cout << "POST status: " << resp.status << ", elapsed: " << resp.elapsed_ms << "ms\n";
        assert(resp.status == 200);
        assert(!resp.body.empty());
        assert(!resp.headers.empty());
    } catch (const std::exception& e) {
        std::cerr << "POST request failed: " << e.what() << std::endl;
        return 1;
    }

    // Test PUT request
    try {
        std::map<std::string, std::string> headers = {{"Content-Type", "application/json"}};
        std::string putData = "{\"test\":\"value\"}";
        HttpClient::HttpResponse resp = client.put("https://httpbin.org/put", headers, putData);
        std::cout << "PUT status: " << resp.status << ", elapsed: " << resp.elapsed_ms << "ms\n";
        assert(resp.status == 200);
        assert(!resp.body.empty());
        assert(!resp.headers.empty());
    } catch (const std::exception& e) {
        std::cerr << "PUT request failed: " << e.what() << std::endl;
        return 1;
    }

    // Test DELETE request
    try {
        HttpClient::HttpResponse resp = client.del("https://httpbin.org/delete");
        std::cout << "DELETE status: " << resp.status << ", elapsed: " << resp.elapsed_ms << "ms\n";
        assert(resp.status == 200);
        assert(!resp.body.empty());
        assert(!resp.headers.empty());
    } catch (const std::exception& e) {
        std::cerr << "DELETE request failed: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "All httpClient tests passed!" << std::endl;
    return 0;
}
