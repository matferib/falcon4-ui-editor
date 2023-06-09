#include "FalconWindow.h"

#include <algorithm> 
#include <cctype>
#include <fstream>
#include <functional> 
#include <locale>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "imgui.h"

namespace falcon_ui {

namespace {

const char* kSeparators = " \t\n\r\f\v";

// trim from end of string (right)
std::string& rtrim(std::string& s, const char* t = kSeparators) {
    s.erase(s.find_last_not_of(t) + 1);
    return s;
}

// trim from beginning of string (left)
std::string& ltrim(std::string& s, const char* t = kSeparators) {
    s.erase(0, s.find_first_not_of(t));
    return s;
}

// trim from both ends of string (right then left)
std::string& TrimLine(std::string& s, const char* t = kSeparators) {
    return ltrim(rtrim(s, t), t);
}

bool IsComment(const std::string& line) {
    return !line.empty() && line[0] == '#';
}

constexpr int kMaxX = 10000;
constexpr int kMaxY = 10000;

//---------------
// Window Element
//---------------
class WindowElement : public Element {
public:
    ElementType Type() const override { return ElementType::WINDOW; }

    bool Setup() override {
        if (attributes_.empty()) return false;
        if (attributes_.at(0).find("[SETUP]") != 0) return false;
        // Sample: [SETUP] UI_MAIN_SCREEN C_TYPE_NORMAL 1024 768
        std::string main_type, label, window_ctype;
        std::stringstream ss(attributes_.at(0));
        ss >> main_type >> label >> window_ctype >> width_ >> height_;
        if (width_ <= 0 || height_ <= 0 || width_ >= kMaxX || height_ >= kMaxY) return false;
        for (auto& child : children_) {
            child->Setup();
        }
        return true;        
    }

    void Draw() const override {
        ImGuiWindowFlags window_flags = 0;

        /*window_flags |= ImGuiWindowFlags_NoTitleBar;
        window_flags |= ImGuiWindowFlags_NoScrollbar;
        window_flags |= ImGuiWindowFlags_MenuBar;
        window_flags |= ImGuiWindowFlags_NoMove;
        window_flags |= ImGuiWindowFlags_NoResize;
        window_flags |= ImGuiWindowFlags_NoCollapse;
        window_flags |= ImGuiWindowFlags_NoNav;
        window_flags |= ImGuiWindowFlags_NoBackground;
        window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
        window_flags |= ImGuiWindowFlags_UnsavedDocument;
        p_open = NULL; // Don't pass our bool* to Begin*/


        // Main body of the Demo window starts here.
        ImGui::SetNextWindowSize(ImVec2(width_, height_));
        ImGui::Begin("WindowElement", nullptr, window_flags);
        for (auto& child : children_) {
            child->Draw();
        }
        ImGui::End();
    }

private:
    int width_, height_;
};

class ButtonElement : public Element {
public:
    ElementType Type() const override { return ElementType::BUTTON; }

    bool Setup() override {
        if (attributes_.empty()) return false;
        if (attributes_.at(0).find("[SETUP]") != 0) return false;
        // Sample: [SETUP] IA_MAIN_CTRL C_TYPE_NORMAL 12 14
        std::string main_type, label, window_ctype;
        std::stringstream ss(attributes_.at(0));
        ss >> main_type >> label >> window_ctype >> x_ >> y_;
        if (x_ <= -kMaxX || y_ <= -kMaxY || x_ >= kMaxX || y_ >= kMaxY) return false;
        return true;
    }

    void Draw() const override {
        ImGui::SetCursorPos(ImVec2(x_, y_));
        ImGui::Button("BUTTONTEXT");
    }

private:
    int x_, y_;
};

class PlaceholderElement : public Element {
public:
    bool Setup() override { return true; }
    ElementType Type() const override { return ElementType::UNKNOWN; }
};

const std::unordered_map<std::string, Element::ElementType> kValidElementsMap = {
     {"[WINDOW]", Element::ElementType::WINDOW}, {"[BITMAP]", Element::ElementType::BITMAP}, {"[TILE]", Element::ElementType::TILE}, {"[BUTTON]", Element::ElementType::BUTTON }
};

}  // namespace

//--------
// Element
//--------

// Param line is updated with the last token read.
// Returns true if successful.
bool Element::Parse(std::string& line, std::istream& stream) {
    while (std::getline(stream, line)) {
        TrimLine(line);
        if (line.empty()) continue;
        if (IsComment(line)) continue;
        if (kValidElementsMap.contains(line)) {
            // A new element started. This is our end.
            good_ = true;
            return true;
        }
        if (!ParseSubElement(line)) {
            // Error reading subelement.
            return false;
        }
    }
    // File is over.
    line.clear();
    good_ = true;
    return true;
}

bool Element::ParseSubElement(const std::string& line) {
    std::unordered_set<std::string> valid_subelements = { "[SETUP]", "[XY]", "[RANGES]", "[GROUP]", "[FLAGBITON]", "[DEPTH]", "[BITMAP]", "[TILE]", "[XYWH]", "[BUTTONIMAGE]", "[BUTTONTEXT]", "[SOUNDBITE]", "[CURSOR]" };
    auto closing_bracket = line.find(']');
    if (closing_bracket == std::string::npos || !valid_subelements.contains(line.substr(0, closing_bracket + 1))) {
        return false;
    }
    attributes_.push_back(line);
    return true;
}

//-------------------------------------
// Window (and its auxiliary functions).
//-------------------------------------

namespace {

// Returns nullptr in case of error. Updates line to be the same as the last line consumed.
std::unique_ptr<Element> ParseElement(std::string& line, std::istream& stream) {
    auto type_result = kValidElementsMap.find(line);
    if (type_result == kValidElementsMap.end()) return {};
    auto type = type_result->second;
    std::unique_ptr<Element> element;
    switch (type) {
        case Element::ElementType::WINDOW: element = std::make_unique<WindowElement>(); break;
        case Element::ElementType::BUTTON: element = std::make_unique<ButtonElement>(); break;
        case Element::ElementType::UNKNOWN: break;
        default: element = std::make_unique<PlaceholderElement>(); break;
    }
    if (element == nullptr) return nullptr;
    if (!element->Parse(line, stream)) return nullptr;    
    return std::move(element);
}

}  // namespace


void Window::SetupFromFile(const std::string& filename) {
    std::ifstream file(filename);
    SetupFromContents(file);
    file.close();
}

void Window::SetupFromContents(std::istream& stream) {
    Parse(stream);

    // Sanity checks.
    if (root_element_ == nullptr || root_element_->Type() != Element::ElementType::WINDOW) {
        good_ = false;
        return;
    }
    
    if (!root_element_->Setup()) {
        good_ = false; 
        return;
    }   
}

void Window::Parse(std::istream& stream) {
    done_ = true;
    std::string line;
    std::vector<std::string> comments;
    if (!std::getline(stream, line)) return;

    good_ = true;
    while (true) {
        TrimLine(line);
        if (line.empty()) {
            if (!std::getline(stream, line)) return;  // EOF.
            continue;
        }
        if (IsComment(line)) {
            comments.push_back(line);
            if (!std::getline(stream, line)) return;  // EOF.
            continue;
        }
        auto element = ParseElement(line, stream);
        if (element == nullptr) {
            // Some kind of error.
            good_ = false;
            return;
        }
        element->SetComments(comments);
        comments.clear();
        if (root_element_ == nullptr) {
            root_element_ = std::move(element);
        } else {
            root_element_->AddChild(std::move(element));
        }
        if (line.empty()) return;  // An element only ends on a new element (not empty line) or EOF (empty line).
    }
}

void Window::Draw() const {
    // We specify a default position/size in case there's no data in the .ini file.
    // We only do it to make the demo applications a little more welcoming, but typically this isn't required.
    const ImGuiViewport* main_viewport = ImGui::GetMainViewport();
    const int root_x = 200, root_y = 200;
    ImGui::SetNextWindowPos(ImVec2(main_viewport->WorkPos.x + root_x, main_viewport->WorkPos.y + root_y));
    root_element_->Draw();
}

}  // namespace falcon_ui