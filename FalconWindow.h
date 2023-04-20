#pragma once

#include <istream>
#include <vector>
#include <string>
#include <vector>


namespace falcon_ui {

class Element {
public:
    enum class ElementType {
        UNKNOWN,
        WINDOW,
        BITMAP,
        TILE,
        BUTTON,
    };

    virtual ~Element() {}
    // Parses the element, starting from line and then consuming stream. Assumes the first line (with type) has been consumed already.
    // Param line is updated with the last line read from stream.
    // Returns true if successful.
    virtual bool Parse(std::string& line, std::istream& stream);

    // Setup the element after it is parsed.
    virtual bool Setup() = 0;

    // Set the high level comments for the element.
    void SetComments(const std::vector<std::string>& comments) { comments_ = comments; }

    virtual void Draw() const {}

    // Returns the type of the element (WINDOW)
    virtual ElementType Type() const = 0;

    void AddChild(std::unique_ptr<Element> element) { children_.push_back(std::move(element)); }

protected:
    std::vector<std::unique_ptr<Element>> children_; 
    std::vector<std::string> attributes_;
    bool good_ = false;
    std::vector<std::string> comments_;

private:
    bool ParseSubElement(const std::string& line);
};

class Window {
public:
    void SetupFromFile(const std::string& filename);
    void SetupFromContents(std::istream& stream);

    bool SetupDone() const { return done_; }
    
    void Draw() const;

private:
    void Parse(std::istream& stream);

    std::unique_ptr<Element> root_element_;
    bool done_ = false;
    bool good_ = false;
};


}  // namespace falcon_ui