#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <memory>

// Immutable original-resolution decode, prepared off the UI/render threads.
class NotebookImage {
public:
    explicit NotebookImage(std::string_view base64);
    static std::shared_ptr<const NotebookImage> Load(const std::wstring& path);
    unsigned Width() const;
    unsigned Height() const;
    const std::vector<unsigned char>& Pixels() const;
    const std::vector<unsigned char>& Profile() const;
private:
    explicit NotebookImage(std::vector<unsigned char> bytes);
    void Decode(std::vector<unsigned char>& bytes);
    unsigned width=0,height=0;
    std::vector<unsigned char> pixels,profile;
};
