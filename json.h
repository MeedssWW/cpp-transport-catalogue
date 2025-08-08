#pragma once

#include <iostream>
#include <map>
#include <string>
#include <variant>
#include <vector>

namespace json {

class Node;
using Dict = std::map<std::string, Node>;
using Array = std::vector<Node>;

class ParsingError : public std::runtime_error {
public:
    using runtime_error::runtime_error;
};

class Node {
public:
    using Value = std::variant<std::nullptr_t, Array, Dict, bool, int, double, std::string>;
    
    Node() : value_(nullptr) {}
    explicit Node(Array array);
    explicit Node(Dict map);
    Node(int value);
    Node(double value);
    Node(bool value);
    Node(std::string value);
    Node(const char* value);
    explicit Node(std::nullptr_t);

    
    template <typename T>
    Node(T&& value) : value_(std::forward<T>(value)) {}

    bool IsNull() const;
    bool IsInt() const;
    bool IsDouble() const;
    bool IsPureDouble() const;
    bool IsBool() const;
    bool IsString() const;
    bool IsArray() const;
    bool IsMap() const;

    int AsInt() const;
    bool AsBool() const;
    double AsDouble() const;
    const std::string& AsString() const;
    const Array& AsArray() const;
    const Dict& AsMap() const;

    const Value& GetValue() const { return value_; }

    bool operator==(const Node& rhs) const;
    bool operator!=(const Node& rhs) const;

    
    bool operator==(const Array& rhs) const;
    bool operator==(const Dict& rhs) const;

private:
    Value value_;
};

class Document {
public:
    explicit Document(Node root);
    const Node& GetRoot() const;

    bool operator==(const Document& rhs) const;
    bool operator!=(const Document& rhs) const;

private:
    Node root_;
};

Document Load(std::istream& input);
void Print(const Document& doc, std::ostream& output);


Node MakeNode(int value);
Node MakeNode(double value);
Node MakeNode(bool value);
Node MakeNode(std::string value);
Node MakeNode(const char* value);
Node MakeNode(Array array);
Node MakeNode(Dict map);


}  // namespace json