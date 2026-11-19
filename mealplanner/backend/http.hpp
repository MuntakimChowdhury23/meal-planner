#pragma once
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <csignal>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

struct Request {
    std::string method, path, body;
    std::map<std::string, std::string> query, headers;
    std::vector<std::string> params;
};

struct Response {
    int status = 200;
    std::string type = "application/json";
    std::string body;
};

static std::string urlDecode(const std::string& s) {
    std::string r;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '%' && i + 2 < s.size()) { r += char(strtol(s.substr(i + 1, 2).c_str(), nullptr, 16)); i += 2; }
        else if (s[i] == '+') r += ' ';
        else r += s[i];
    }
    return r;
}

class HttpServer {
public:
    using Handler = std::function<Response(const Request&)>;

    void route(const std::string& method, const std::string& pattern, Handler h) {
        routes_.push_back({method, split(pattern), h});
    }
    void staticDir(const std::string& dir) { static_ = dir; }

    bool listenOn(int port) {
        signal(SIGPIPE, SIG_IGN);
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        int yes = 1;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = htons(port);
        if (bind(fd, (sockaddr*)&addr, sizeof addr) < 0 || listen(fd, 64) < 0) { perror("bind/listen"); return false; }
        std::cout << "MealPlanner server listening on http://0.0.0.0:" << port << std::endl;
        while (true) {
            int c = accept(fd, nullptr, nullptr);
            if (c < 0) continue;
            std::thread([this, c] { serve(c); }).detach();
        }
    }

private:
    struct Route { std::string method; std::vector<std::string> parts; Handler h; };
    std::vector<Route> routes_;
    std::string static_;

    static std::vector<std::string> split(const std::string& p) {
        std::vector<std::string> r; std::string cur;
        for (char c : p) { if (c == '/') { if (!cur.empty()) r.push_back(cur); cur.clear(); } else cur += c; }
        if (!cur.empty()) r.push_back(cur);
        return r;
    }

    static std::string mime(const std::string& p) {
        auto ends = [&](const char* e) { size_t l = strlen(e); return p.size() >= l && p.compare(p.size() - l, l, e) == 0; };
        if (ends(".html")) return "text/html; charset=utf-8";
        if (ends(".css")) return "text/css";
        if (ends(".js")) return "application/javascript";
        if (ends(".json")) return "application/json";
        if (ends(".svg")) return "image/svg+xml";
        if (ends(".png")) return "image/png";
        if (ends(".webmanifest")) return "application/manifest+json";
        return "application/octet-stream";
    }

    void serve(int c) {
        std::string data;
        char buf[8192];
        size_t headerEnd = std::string::npos;
        while (headerEnd == std::string::npos) {
            ssize_t n = recv(c, buf, sizeof buf, 0);
            if (n <= 0) { close(c); return; }
            data.append(buf, n);
            headerEnd = data.find("\r\n\r\n");
            if (data.size() > (1 << 20)) { close(c); return; }
        }
        Request req;
        std::istringstream hs(data.substr(0, headerEnd));
        std::string line, target, ver;
        std::getline(hs, line);
        std::istringstream rl(line);
        rl >> req.method >> target >> ver;
        while (std::getline(hs, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            auto p = line.find(':');
            if (p == std::string::npos) continue;
            std::string k = line.substr(0, p), v = line.substr(p + 1);
            while (!v.empty() && v[0] == ' ') v.erase(0, 1);
            for (auto& ch : k) ch = tolower(ch);
            req.headers[k] = v;
        }
        size_t need = req.headers.count("content-length") ? (size_t)atol(req.headers["content-length"].c_str()) : 0;
        if (need > (4u << 20)) { close(c); return; }
        req.body = data.substr(headerEnd + 4);
        while (req.body.size() < need) {
            ssize_t n = recv(c, buf, sizeof buf, 0);
            if (n <= 0) break;
            req.body.append(buf, n);
        }
        auto q = target.find('?');
        req.path = urlDecode(target.substr(0, q));
        if (q != std::string::npos) {
            std::istringstream qs(target.substr(q + 1));
            std::string kv;
            while (std::getline(qs, kv, '&')) {
                auto e = kv.find('=');
                if (e == std::string::npos) req.query[urlDecode(kv)] = "";
                else req.query[urlDecode(kv.substr(0, e))] = urlDecode(kv.substr(e + 1));
            }
        }
        Response res = dispatch(req);
        std::ostringstream out;
        out << "HTTP/1.1 " << res.status << (res.status == 200 ? " OK" : res.status == 404 ? " Not Found" : res.status == 400 ? " Bad Request" : " Error")
            << "\r\nContent-Type: " << res.type << "\r\nContent-Length: " << res.body.size()
            << "\r\nCache-Control: no-cache\r\nConnection: close\r\n\r\n" << res.body;
        std::string s = out.str();
        size_t sent = 0;
        while (sent < s.size()) {
            ssize_t n = send(c, s.data() + sent, s.size() - sent, 0);
            if (n <= 0) break;
            sent += n;
        }
        close(c);
    }

    Response dispatch(const Request& req) {
        auto parts = split(req.path);
        for (auto& r : routes_) {
            if (r.method != req.method || r.parts.size() != parts.size()) continue;
            Request rq = req;
            bool ok = true;
            for (size_t i = 0; i < parts.size() && ok; i++) {
                if (!r.parts[i].empty() && r.parts[i][0] == ':') rq.params.push_back(parts[i]);
                else if (r.parts[i] != parts[i]) ok = false;
            }
            if (!ok) continue;
            try { return r.h(rq); }
            catch (const std::exception& e) { return {400, "application/json", std::string("{\"error\":") + Json_esc(e.what()) + "}"}; }
        }
        if (req.method == "GET" && !static_.empty()) {
            std::string p = req.path;
            if (p == "/") p = "/index.html";
            if (p.find("..") == std::string::npos) {
                std::ifstream f(static_ + p, std::ios::binary);
                if (f) { std::stringstream ss; ss << f.rdbuf(); return {200, mime(p), ss.str()}; }
            }
        }
        return {404, "application/json", "{\"error\":\"not found\"}"};
    }

    static std::string Json_esc(const std::string& in) {
        std::string r = "\"";
        for (unsigned char c : in) { if (c == '"' || c == '\\') { r += '\\'; r += char(c); } else if (c >= 0x20) r += char(c); }
        return r + "\"";
    }
};
