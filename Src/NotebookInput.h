#pragma once
#include <array>
#include <fstream>
#include <functional>
#include <iterator>
#include <string>
#include "../ThirdParty/nlohmann/json.hpp"

// Bounded input buffering; parsing never retains a second copy of the file bytes.
class NotebookInput final {
public:
    explicit NotebookInput(const std::wstring& path, std::function<bool()> cancelled = {});
    NotebookInput(const NotebookInput&) = delete;
    NotebookInput& operator=(const NotebookInput&) = delete;
    nlohmann::json Read();
private:
    // Single-pass iterator over the current block. Keep its implementation with
    // Read so the JSON parser can inline byte access without streambuf overhead.
    class Iterator {
    public:
        using iterator_category = std::input_iterator_tag;
        using value_type = char;
        using difference_type = std::ptrdiff_t;
        using pointer = const char*;
        using reference = const char&;
        struct Previous {
            char value;
            char operator*() const;
        };
        Iterator();
        explicit Iterator(NotebookInput* input);
        reference operator*() const;
        Iterator& operator++();
        Previous operator++(int);
        bool operator==(const Iterator& other) const;
        bool operator!=(const Iterator& other) const;
    private:
        NotebookInput* input = nullptr;
        const char* current = nullptr;
        const char* end = nullptr;
    };
    size_t ReadBlock();
    std::ifstream file;
    std::function<bool()> cancelled;
    std::array<char, 65536> buffer;
};
