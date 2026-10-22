#pragma once
#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <cctype>
#include <iomanip>

namespace mini_json {

enum class Type {
    Null,
    Boolean,
    Number,
    String,
    Array,
    Object
};

class Value {
public:
    Type type = Type::Null;
    bool bool_val = false;
    double num_val = 0.0;
    std::string str_val;
    std::vector<Value> arr_val;
    std::map<std::string, Value> obj_val;

    Value() : type(Type::Null) {}
    Value(bool b) : type(Type::Boolean), bool_val(b) {}
    Value(int n) : type(Type::Number), num_val((double)n) {}
    Value(double n) : type(Type::Number), num_val(n) {}
    Value(const char* s) : type(Type::String), str_val(s ? s : "") {}
    Value(const std::string& s) : type(Type::String), str_val(s) {}

    bool is_null() const { return type == Type::Null; }
    bool is_bool() const { return type == Type::Boolean; }
    bool is_number() const { return type == Type::Number; }
    bool is_string() const { return type == Type::String; }
    bool is_array() const { return type == Type::Array; }
    bool is_object() const { return type == Type::Object; }

    std::string as_string(const std::string& def = "") const {
        return (type == Type::String) ? str_val : def;
    }

    double as_number(double def = 0.0) const {
        return (type == Type::Number) ? num_val : def;
    }

    int as_int(int def = 0) const {
        return (type == Type::Number) ? (int)num_val : def;
    }

    bool as_bool(bool def = false) const {
        return (type == Type::Boolean) ? bool_val : def;
    }

    bool has(const std::string& key) const {
        return type == Type::Object && obj_val.find(key) != obj_val.end();
    }

    Value& operator[](const std::string& key) {
        if (type != Type::Object) {
            type = Type::Object;
            obj_val.clear();
        }
        return obj_val[key];
    }

    const Value& operator[](const std::string& key) const {
        static Value null_v;
        if (type == Type::Object) {
            auto it = obj_val.find(key);
            if (it != obj_val.end()) return it->second;
        }
        return null_v;
    }

    Value& operator[](size_t index) {
        if (type != Type::Array) {
            type = Type::Array;
            arr_val.clear();
        }
        if (index >= arr_val.size()) arr_val.resize(index + 1);
        return arr_val[index];
    }

    const Value& operator[](size_t index) const {
        static Value null_v;
        if (type == Type::Array && index < arr_val.size()) {
            return arr_val[index];
        }
        return null_v;
    }

    void push_back(const Value& val) {
        if (type != Type::Array) {
            type = Type::Array;
            arr_val.clear();
        }
        arr_val.push_back(val);
    }

    std::string serialize(int indent = 0) const {
        std::string ind(indent * 2, ' ');
        switch (type) {
            case Type::Null: return "null";
            case Type::Boolean: return bool_val ? "true" : "false";
            case Type::Number: {
                std::ostringstream ss;
                if (num_val == (long long)num_val) ss << (long long)num_val;
                else ss << std::fixed << std::setprecision(2) << num_val;
                return ss.str();
            }
            case Type::String: {
                std::string s = "\"";
                for (char c : str_val) {
                    if (c == '"') s += "\\\"";
                    else if (c == '\\') s += "\\\\";
                    else if (c == '\n') s += "\\n";
                    else if (c == '\r') s += "\\r";
                    else if (c == '\t') s += "\\t";
                    else s += c;
                }
                s += "\"";
                return s;
            }
            case Type::Array: {
                if (arr_val.empty()) return "[]";
                std::string s = "[\n";
                for (size_t i = 0; i < arr_val.size(); ++i) {
                    s += ind + "  " + arr_val[i].serialize(indent + 1);
                    if (i + 1 < arr_val.size()) s += ",";
                    s += "\n";
                }
                s += ind + "]";
                return s;
            }
            case Type::Object: {
                if (obj_val.empty()) return "{}";
                std::string s = "{\n";
                size_t count = 0;
                for (auto& pair : obj_val) {
                    s += ind + "  \"" + pair.first + "\": " + pair.second.serialize(indent + 1);
                    if (++count < obj_val.size()) s += ",";
                    s += "\n";
                }
                s += ind + "}";
                return s;
            }
        }
        return "null";
    }

    static Value parse(const std::string& str) {
        size_t pos = 0;
        return parse_value(str, pos);
    }

private:
    static void skip_whitespace(const std::string& str, size_t& pos) {
        while (pos < str.size() && (std::isspace((unsigned char)str[pos]))) {
            pos++;
        }
    }

    static Value parse_value(const std::string& str, size_t& pos) {
        skip_whitespace(str, pos);
        if (pos >= str.size()) return Value();

        char c = str[pos];
        if (c == '{') return parse_object(str, pos);
        if (c == '[') return parse_array(str, pos);
        if (c == '"') return parse_string(str, pos);
        if (c == 't' || c == 'f') return parse_boolean(str, pos);
        if (c == 'n') return parse_null(str, pos);
        if (c == '-' || std::isdigit((unsigned char)c)) return parse_number(str, pos);

        return Value();
    }

    static Value parse_object(const std::string& str, size_t& pos) {
        Value val;
        val.type = Type::Object;
        pos++; // skip '{'

        while (pos < str.size()) {
            skip_whitespace(str, pos);
            if (pos < str.size() && str[pos] == '}') {
                pos++;
                return val;
            }

            if (pos >= str.size() || str[pos] != '"') break;
            Value key = parse_string(str, pos);
            skip_whitespace(str, pos);
            if (pos >= str.size() || str[pos] != ':') break;
            pos++; // skip ':'

            Value subVal = parse_value(str, pos);
            val.obj_val[key.str_val] = subVal;

            skip_whitespace(str, pos);
            if (pos < str.size() && str[pos] == ',') {
                pos++;
            } else if (pos < str.size() && str[pos] == '}') {
                pos++;
                return val;
            }
        }
        return val;
    }

    static Value parse_array(const std::string& str, size_t& pos) {
        Value val;
        val.type = Type::Array;
        pos++; // skip '['

        while (pos < str.size()) {
            skip_whitespace(str, pos);
            if (pos < str.size() && str[pos] == ']') {
                pos++;
                return val;
            }

            Value subVal = parse_value(str, pos);
            val.arr_val.push_back(subVal);

            skip_whitespace(str, pos);
            if (pos < str.size() && str[pos] == ',') {
                pos++;
            } else if (pos < str.size() && str[pos] == ']') {
                pos++;
                return val;
            }
        }
        return val;
    }

    static Value parse_string(const std::string& str, size_t& pos) {
        Value val;
        val.type = Type::String;
        pos++; // skip opening '"'
        std::string s;
        while (pos < str.size()) {
            char c = str[pos++];
            if (c == '"') {
                val.str_val = s;
                return val;
            }
            if (c == '\\' && pos < str.size()) {
                char esc = str[pos++];
                if (esc == '"') s += '"';
                else if (esc == '\\') s += '\\';
                else if (esc == 'n') s += '\n';
                else if (esc == 'r') s += '\r';
                else if (esc == 't') s += '\t';
                else s += esc;
            } else {
                s += c;
            }
        }
        val.str_val = s;
        return val;
    }

    static Value parse_number(const std::string& str, size_t& pos) {
        size_t start = pos;
        if (str[pos] == '-') pos++;
        while (pos < str.size() && std::isdigit((unsigned char)str[pos])) pos++;
        if (pos < str.size() && str[pos] == '.') {
            pos++;
            while (pos < str.size() && std::isdigit((unsigned char)str[pos])) pos++;
        }
        std::string num_str = str.substr(start, pos - start);
        double d = std::stod(num_str);
        return Value(d);
    }

    static Value parse_boolean(const std::string& str, size_t& pos) {
        if (str.compare(pos, 4, "true") == 0) {
            pos += 4;
            return Value(true);
        }
        if (str.compare(pos, 5, "false") == 0) {
            pos += 5;
            return Value(false);
        }
        return Value();
    }

    static Value parse_null(const std::string& str, size_t& pos) {
        if (str.compare(pos, 4, "null") == 0) {
            pos += 4;
        }
        return Value();
    }
};

} // namespace mini_json
