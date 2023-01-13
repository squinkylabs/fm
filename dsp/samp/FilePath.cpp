
#include "FilePath.h"

#include <assert.h>

#include <algorithm>

FilePath::FilePath(const char* s) : data(s) {
    fixSeparators();
    initAbs();
}

FilePath::FilePath(const std::string& s) : data(s) {
    fixSeparators();
    initAbs();
}

void FilePath::initAbs() {
    if (data.empty()) {
        return;
    }
    if (data.size() >= 2 && data[1] == ':') {
        abs= true;
    }
    if (data[0] ==  '\\') {
        abs = true;
    }
    if (data[0] == '/') {
        abs = true;
    }
}

std::string FilePath::toString() const {
    return data;
}

void FilePath::fixSeparators() {
    std::replace(data.begin(), data.end(), foreignSeparator(), nativeSeparator());
}

void FilePath::concat(const FilePath& _other) {
    // slihgtly special case / bug if we are empty
    if (this->empty()) {
        this->data = _other.data;
        return;
    }

    FilePath other = _other;

    // remove any leading dots from other
    const bool secondStartsWithDot = other.startsWithDot();
    if (secondStartsWithDot) {
        other = FilePath(other.toString().substr(1));
    }

    // if second was just a dot, do nothing
    if (other.empty()) {
        return;
    }

    {
        const bool firstEndsWithSeparator = this->endsWithSeparator();
        const bool secondStartsWithSeparator = other.startsWithSeparator();

        if (firstEndsWithSeparator && secondStartsWithSeparator) {
            data.pop_back();
        } else if (firstEndsWithSeparator || secondStartsWithSeparator) {
        } else {
            data += nativeSeparator();
        }
        data += other.data;
    }
}

bool FilePath::empty() const {
    return data.empty();
}

bool FilePath::startsWithSeparator() const {
    if (data.empty()) {
        return false;
    }
    return data.at(0) == nativeSeparator();
}

bool FilePath::endsWithSeparator() const {
    if (data.empty()) {
        return false;
    }
    return data.back() == nativeSeparator();
}

bool FilePath::startsWithDot() const {
    if (data.empty()) {
        return false;
    }

    // don't return true for ".."
    const bool dotAtZero = data.at(0) == '.';
    const bool dotAtOne = (data.size() < 2) ? false : (data.at(1) == '.');
    return dotAtZero && !dotAtOne;
}

char FilePath::nativeSeparator() {
#ifdef ARCH_WIN
    return '\\';
#else
    return '/';
#endif
}

char FilePath::foreignSeparator() {
#ifdef ARCH_WIN
    return '/';
#else
    return '\\';
#endif
}

FilePath FilePath::getPathPart() const {
    std::string s = toString();
    auto pos = s.rfind(nativeSeparator());
    if (pos == std::string::npos) {
        return FilePath("");
    }
    auto subPath = s.substr(0, pos);
    return FilePath(subPath);
}

std::string FilePath::getFilenamePart() const {
    std::string s = toString();
    auto pos = s.rfind(nativeSeparator());
    if (pos == std::string::npos) {
        return s;  // return the whole thing if no separator found
    }
    return s.substr(pos + 1);
}

std::string FilePath::getFilenamePartNoExtension() const {
    std::string s = getFilenamePart();
    auto pos = s.rfind('.');
    return pos == std::string::npos ? s : s.substr(0, pos);
}

std::string FilePath::getExtensionLC() const {
    std::string s = getFilenamePart();
    auto pos = s.rfind('.');
    s = (pos == std::string::npos) ? "" : s.substr(pos + 1);

    //  td::string data = "Abc";
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return s;
}
