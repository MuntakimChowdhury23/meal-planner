#pragma once
#include <map>
#include <string>
#include <vector>
#include <sstream>
#include <stdexcept>
#include <cmath>
#include <cstdio>

struct Json {
    enum Type { Null, Bool, Num, Str, Arr, Obj } t = Null;
    bool b = false;
    double n = 0;
    std::string s;
    std::vector<Json> a;
    std::map<std::string, Json> o;

    Json() {}
    Json(std::nullptr_t) {}
    Json(bool v) : t(Bool), b(v) {}
    Json(int v) : t(Num), n(v) {}
    Json(long v) : t(Num), n(double(v)) {}
    Json(size_t v) : t(Num), n(double(v)) {}
    Json(double v) : t(Num), n(v) {}
    Json(const char* v) : t(Str), s(v) {}
    Json(const std::string& v) : t(Str), s(v) {}

    static Json array() { Json j; j.t = Arr; return j; }
    static Json object() { Json j; j.t = Obj; return j; }

    Json& operator[](const std::string& k) { if (t != Obj) { t = Obj; o.clear(); } return o[k]; }
    const Json& at(const std::string& k) const {
        static const Json nul;
        auto it = o.find(k);
        return (t == Obj && it != o.end()) ? it->second : nul;
    }
    bool has(const std::string& k) const { return t == Obj && o.count(k); }
    void push(const Json& v) { if (t != Arr) { t = Arr; a.clear(); } a.push_back(v); }

    double num(double d = 0) const { return t == Num ? n : (t == Str ? atof(s.c_str()) : d); }
    int integer(int d = 0) const { return t == Num || t == Str ? (int)std::lround(num()) : d; }
    std::string str(const std::string& d = "") const { return t == Str ? s : d; }
    bool boolean(bool d = false) const { return t == Bool ? b : d; }
    std::vector<std::string> strList() const {
        std::vector<std::string> r;
        if (t == Arr) for (auto& x : a) if (x.t == Str) r.push_back(x.s);
        return r;
    }

    static std::string esc(const std::string& in) {
        std::string r = "\"";
        for (unsigned char c : in) {
            switch (c) {
                case '"': r += "\\\""; break;
                case '\\': r += "\\\\"; break;
                case '\n': r += "\\n"; break;
                case '\r': r += "\\r"; break;
                case '\t': r += "\\t"; break;
                default:
                    if (c < 0x20) { char buf[8]; snprintf(buf, sizeof buf, "\\u%04x", c); r += buf; }
                    else r += char(c);
            }
        }
        return r + "\"";
    }

    std::string dump() const {
        switch (t) {
            case Null: return "null";
            case Bool: return b ? "true" : "false";
            case Num: {
                if (std::isfinite(n) && n == std::floor(n) && std::fabs(n) < 1e15) {
                    char buf[32]; snprintf(buf, sizeof buf, "%lld", (long long)n); return buf;
                }
                char buf[40]; snprintf(buf, sizeof buf, "%.4f", std::isfinite(n) ? n : 0.0);
                std::string r = buf;
                while (!r.empty() && r.back() == '0') r.pop_back();
                if (!r.empty() && r.back() == '.') r.pop_back();
                return r;
            }
            case Str: return esc(s);
            case Arr: {
                std::string r = "[";
                for (size_t i = 0; i < a.size(); i++) { if (i) r += ","; r += a[i].dump(); }
                return r + "]";
            }
            case Obj: {
                std::string r = "{";
                bool first = true;
                for (auto& kv : o) { if (!first) r += ","; first = false; r += esc(kv.first) + ":" + kv.second.dump(); }
                return r + "}";
            }
        }
        return "null";
    }

    static Json parse(const std::string& src) {
        size_t i = 0;
        Json v = parseValue(src, i);
        return v;
    }

private:
    static void ws(const std::string& s, size_t& i) { while (i < s.size() && isspace((unsigned char)s[i])) i++; }

    static Json parseValue(const std::string& s, size_t& i) {
        ws(s, i);
        if (i >= s.size()) throw std::runtime_error("unexpected end of JSON");
        char c = s[i];
        if (c == '{') {
            Json j = object(); i++; ws(s, i);
            if (i < s.size() && s[i] == '}') { i++; return j; }
            while (true) {
                ws(s, i);
                std::string k = parseString(s, i);
                ws(s, i);
                if (i >= s.size() || s[i] != ':') throw std::runtime_error("expected ':'");
                i++;
                j.o[k] = parseValue(s, i);
                ws(s, i);
                if (i < s.size() && s[i] == ',') { i++; continue; }
                if (i < s.size() && s[i] == '}') { i++; break; }
                throw std::runtime_error("bad object");
            }
            return j;
        }
        if (c == '[') {
            Json j = array(); i++; ws(s, i);
            if (i < s.size() && s[i] == ']') { i++; return j; }
            while (true) {
                j.a.push_back(parseValue(s, i));
                ws(s, i);
                if (i < s.size() && s[i] == ',') { i++; continue; }
                if (i < s.size() && s[i] == ']') { i++; break; }
                throw std::runtime_error("bad array");
            }
            return j;
        }
        if (c == '"') return Json(parseString(s, i));
        if (s.compare(i, 4, "true") == 0) { i += 4; return Json(true); }
        if (s.compare(i, 5, "false") == 0) { i += 5; return Json(false); }
        if (s.compare(i, 4, "null") == 0) { i += 4; return Json(); }
        size_t st = i;
        while (i < s.size() && (isdigit((unsigned char)s[i]) || s[i]=='-' || s[i]=='+' || s[i]=='.' || s[i]=='e' || s[i]=='E')) i++;
        if (st == i) throw std::runtime_error("bad JSON value");
        return Json(atof(s.substr(st, i - st).c_str()));
    }

    static void appendUtf8(std::string& out, unsigned cp) {
        if (cp < 0x80) out += char(cp);
        else if (cp < 0x800) { out += char(0xC0 | (cp >> 6)); out += char(0x80 | (cp & 0x3F)); }
        else { out += char(0xE0 | (cp >> 12)); out += char(0x80 | ((cp >> 6) & 0x3F)); out += char(0x80 | (cp & 0x3F)); }
    }

    static std::string parseString(const std::string& s, size_t& i) {
        if (i >= s.size() || s[i] != '"') throw std::runtime_error("expected string");
        i++;
        std::string r;
        while (i < s.size() && s[i] != '"') {
            if (s[i] == '\\' && i + 1 < s.size()) {
                i++;
                switch (s[i]) {
                    case 'n': r += '\n'; break;
                    case 't': r += '\t'; break;
                    case 'r': r += '\r'; break;
                    case 'b': r += '\b'; break;
                    case 'f': r += '\f'; break;
                    case 'u': if (i + 4 < s.size()) { appendUtf8(r, (unsigned)strtoul(s.substr(i + 1, 4).c_str(), nullptr, 16)); i += 4; } break;
                    default: r += s[i];
                }
                i++;
            } else r += s[i++];
        }
        if (i >= s.size()) throw std::runtime_error("unterminated string");
        i++;
        return r;
    }
};
