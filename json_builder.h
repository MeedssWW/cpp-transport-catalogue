#pragma once
#include "json.h"
#include <vector>
#include <optional>
#include <stdexcept>
#include <string>

namespace json {

class Builder;
class DictItemContext;
class KeyItemContext;
class ValueAfterKeyContext;
class ArrayItemContext;

class DictItemContext {
public:
    explicit DictItemContext(Builder& builder);
    KeyItemContext Key(std::string key);
    Builder& EndDict();
private:
    Builder& builder_;
};

class KeyItemContext {
public:
    explicit KeyItemContext(Builder& builder);
    ValueAfterKeyContext Value(Node::Value value);
    DictItemContext StartDict();
    ArrayItemContext StartArray();
private:
    Builder& builder_;
};

class ValueAfterKeyContext {
public:
    explicit ValueAfterKeyContext(Builder& builder);
    KeyItemContext Key(std::string key);
    Builder& EndDict();
private:
    Builder& builder_;
};

class ArrayItemContext {
public:
    explicit ArrayItemContext(Builder& builder);
    ArrayItemContext Value(Node::Value value);
    DictItemContext StartDict();
    ArrayItemContext StartArray();
    Builder& EndArray();
private:
    Builder& builder_;
};

class Builder {
public:
    Builder();
    DictItemContext StartDict();
    ArrayItemContext StartArray();
    Builder& Value(Node::Value value); 
    Node Build();

    KeyItemContext Key(std::string key);
    Builder& EndDict();
    Builder& EndArray();

private:
    friend class DictItemContext;
    friend class KeyItemContext;
    friend class ValueAfterKeyContext;
    friend class ArrayItemContext;

    KeyItemContext DoKey(std::string key);
    ValueAfterKeyContext DoValue(Node::Value value);
    DictItemContext DoStartDict();
    ArrayItemContext DoStartArray();
    Builder& DoEndDict();
    Builder& DoEndArray();
    KeyItemContext DoKeyAfterValue(std::string key);

    Node root_;
    std::vector<Node*> nodes_stack_;
    std::optional<std::string> waiting_key_;
    bool completed_ = false;

    enum class ContextType { None, Dict, Array, KeyAfterDict, ValueAfterKey };
    ContextType context_type_ = ContextType::None;
};
inline DictItemContext::DictItemContext(Builder& builder) : builder_(builder) {}
inline KeyItemContext DictItemContext::Key(std::string key) { return builder_.DoKey(std::move(key)); }
inline Builder& DictItemContext::EndDict() { return builder_.DoEndDict(); }

inline KeyItemContext::KeyItemContext(Builder& builder) : builder_(builder) {}
inline ValueAfterKeyContext KeyItemContext::Value(Node::Value value) { return builder_.DoValue(std::move(value)); }
inline DictItemContext KeyItemContext::StartDict() { return builder_.DoStartDict(); }
inline ArrayItemContext KeyItemContext::StartArray() { return builder_.DoStartArray(); }

inline ValueAfterKeyContext::ValueAfterKeyContext(Builder& builder) : builder_(builder) {}
inline KeyItemContext ValueAfterKeyContext::Key(std::string key) { return builder_.DoKeyAfterValue(std::move(key)); }
inline Builder& ValueAfterKeyContext::EndDict() { return builder_.DoEndDict(); }

inline ArrayItemContext::ArrayItemContext(Builder& builder) : builder_(builder) {}
inline ArrayItemContext ArrayItemContext::Value(Node::Value value) { builder_.Value(std::move(value)); return *this; }
inline DictItemContext ArrayItemContext::StartDict() { return builder_.DoStartDict(); }
inline ArrayItemContext ArrayItemContext::StartArray() { return builder_.DoStartArray(); }
inline Builder& ArrayItemContext::EndArray() { return builder_.DoEndArray(); }

inline Builder::Builder() = default;

inline DictItemContext Builder::StartDict() {
    if (completed_) throw std::logic_error("StartDict after Build or after value already set");
    Node dict_node = Dict{};
    if (nodes_stack_.empty()) {
        root_ = std::move(dict_node);
        nodes_stack_.push_back(&root_);
        return DictItemContext(*this);
    }
    Node* top = nodes_stack_.back();
    if (top->IsArray()) {
        auto& arr = const_cast<Array&>(top->AsArray());
        arr.emplace_back(Dict{});
        nodes_stack_.push_back(&arr.back());
        return DictItemContext(*this);
    }
    if (waiting_key_) {
        auto& dict = const_cast<Dict&>(top->AsMap());
        dict.emplace(*waiting_key_, Dict{});
        auto it = dict.find(*waiting_key_);
        nodes_stack_.push_back(&it->second);
        waiting_key_.reset();
        return DictItemContext(*this);
    }
    throw std::logic_error("StartDict in wrong context");
}

inline ArrayItemContext Builder::StartArray() {
    if (completed_) throw std::logic_error("StartArray after Build or after value already set");
    Node arr_node = Array{};
    if (nodes_stack_.empty()) {
        root_ = std::move(arr_node);
        nodes_stack_.push_back(&root_);
        return ArrayItemContext(*this);
    }
    Node* top = nodes_stack_.back();
    if (top->IsArray()) {
        auto& arr = const_cast<Array&>(top->AsArray());
        arr.emplace_back(Array{});
        nodes_stack_.push_back(&arr.back());
        return ArrayItemContext(*this);
    }
    if (waiting_key_) {
        auto& dict = const_cast<Dict&>(top->AsMap());
        dict.emplace(*waiting_key_, Array{});
        auto it = dict.find(*waiting_key_);
        nodes_stack_.push_back(&it->second);
        waiting_key_.reset();
        return ArrayItemContext(*this);
    }
    throw std::logic_error("StartArray in wrong context");
}

inline Builder& Builder::Value(Node::Value value) {
    if (completed_) throw std::logic_error("Value after Build or after value already set");
    if (!nodes_stack_.empty()) {
        Node* top = nodes_stack_.back();
        if (top->IsArray()) {
            auto& arr = const_cast<Array&>(top->AsArray());
            arr.emplace_back(std::move(value));
            return *this;
        }
        if (waiting_key_) {
            auto& dict = const_cast<Dict&>(top->AsMap());
            dict.emplace(std::move(*waiting_key_), Node(std::move(value)));
            waiting_key_.reset();
            return *this;
        }
        throw std::logic_error("Value in wrong context");
    }
    if (root_.GetValue().index() != 0) throw std::logic_error("Root value already set");
    root_ = Node(std::move(value));
    completed_ = true;
    return *this;
}

inline Node Builder::Build() {
    if (!completed_ || !nodes_stack_.empty())
        throw std::logic_error("Build called before JSON is complete");
    return std::move(root_);
}

inline KeyItemContext Builder::DoKey(std::string key) {
    if (completed_) throw std::logic_error("Key after Build or after value already set");
    if (nodes_stack_.empty() || !nodes_stack_.back()->IsMap())
        throw std::logic_error("Key outside of dict");
    if (waiting_key_) throw std::logic_error("Key after Key");
    waiting_key_ = std::move(key);
    return KeyItemContext(*this);
}

inline ValueAfterKeyContext Builder::DoValue(Node::Value value) {
    if (completed_) throw std::logic_error("Value after Build or after value already set");
    if (!nodes_stack_.empty()) {
        Node* top = nodes_stack_.back();
        if (waiting_key_ && top->IsMap()) {
            auto& dict = const_cast<Dict&>(top->AsMap());
            dict.emplace(std::move(*waiting_key_), Node(std::move(value)));
            waiting_key_.reset();
            return ValueAfterKeyContext(*this);
        }
        throw std::logic_error("Value in wrong context after Key");
    }
    throw std::logic_error("Value in wrong context after Key");
}

inline DictItemContext Builder::DoStartDict() {
    return StartDict();
}

inline ArrayItemContext Builder::DoStartArray() {
    return StartArray();
}

inline Builder& Builder::DoEndDict() {
    if (completed_) throw std::logic_error("EndDict after Build or after value already set");
    if (nodes_stack_.empty() || !nodes_stack_.back()->IsMap())
        throw std::logic_error("EndDict outside of dict");
    if (waiting_key_) throw std::logic_error("EndDict after Key");
    nodes_stack_.pop_back();
    if (nodes_stack_.empty()) completed_ = true;
    return *this;
}

inline Builder& Builder::DoEndArray() {
    if (completed_) throw std::logic_error("EndArray after Build or after value already set");
    if (nodes_stack_.empty() || !nodes_stack_.back()->IsArray())
        throw std::logic_error("EndArray outside of array");
    nodes_stack_.pop_back();
    if (nodes_stack_.empty()) completed_ = true;
    return *this;
}

inline KeyItemContext Builder::DoKeyAfterValue(std::string key) {
    return DoKey(std::move(key));
}

inline KeyItemContext Builder::Key(std::string key) {
    if (nodes_stack_.empty() || !nodes_stack_.back()->IsMap()) {
        throw std::logic_error("Key called in wrong context");
    }
    if (waiting_key_) {
        throw std::logic_error("Key after Key");
    }
    return DoKey(std::move(key));
}

inline Builder& Builder::EndDict() {
    if (nodes_stack_.empty() || !nodes_stack_.back()->IsMap()) {
        throw std::logic_error("EndDict called in wrong context");
    }
    if (waiting_key_) {
        throw std::logic_error("EndDict after Key");
    }
    return DoEndDict();
}

inline Builder& Builder::EndArray() {
    if (nodes_stack_.empty() || !nodes_stack_.back()->IsArray()) {
        throw std::logic_error("EndArray called in wrong context");
    }
    return DoEndArray();
}

} // namespace json