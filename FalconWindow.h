#pragma once

#include <vector>
#include <string>

namespace falcon_ui {


class Element {
public:

};

class WindowSet {
public:
    void SetupFromFile(const std::string& filename);
    void SetupFromContents(const std::string& contents);

    bool SetupDone() const { return done_; }

private:
    std::vector<Element> elements_;
    bool done_ = false;
};


}  // namespace falcon_ui