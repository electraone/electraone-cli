#include "commands/human_format.hpp"

#include <ArduinoJson.h>

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#include <io.h>
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <unistd.h>
#endif

namespace commands {

namespace {

bool stdoutIsTerminal() {
#ifdef _WIN32
    return _isatty(_fileno(stdout)) != 0;
#else
    return isatty(fileno(stdout)) != 0;
#endif
}

size_t terminalWidth() {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info)) {
        return static_cast<size_t>(info.srWindow.Right - info.srWindow.Left + 1);
    }
    return 80;
#else
    struct winsize w{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_col > 0) return w.ws_col;
    return 80;
#endif
}

// Renders a scalar (string/number/bool/null) without JSON string quoting.
std::string scalarText(JsonVariantConst v) {
    if (v.isNull()) return "null";
    if (v.is<const char*>()) return v.as<const char*>();
    std::string s;
    serializeJson(v, s);
    return s;
}

// Renders any value compactly for a table cell: scalars unquoted, nested
// objects/arrays as compact JSON so the table stays one line per row.
std::string cellText(JsonVariantConst v) {
    if (v.is<JsonObjectConst>() || v.is<JsonArrayConst>()) {
        std::string s;
        serializeJson(v, s);
        return s;
    }
    return scalarText(v);
}

void renderTree(std::ostream& os, JsonVariantConst value, int indent);
void renderTable(std::ostream& os, JsonArrayConst arr, int indent);
void renderGrid(std::ostream& os, JsonArrayConst arr, int indent);

void renderTreeArrayValue(std::ostream& os, const std::string& key, JsonArrayConst arr, int indent) {
    std::string pad(static_cast<size_t>(indent) * 2, ' ');
    if (arr.size() == 0) {
        os << pad << key << ": []\n";
        return;
    }

    bool allObjects = true;
    bool allScalar = true;
    for (JsonVariantConst item : arr) {
        if (!item.is<JsonObjectConst>()) allObjects = false;
        if (item.is<JsonObjectConst>() || item.is<JsonArrayConst>()) allScalar = false;
    }

    if (allScalar) {
        // Short, comma-separated inline - a full grid would be overkill for
        // a field nested inside a larger object.
        os << pad << key << ": ";
        bool first = true;
        for (JsonVariantConst item : arr) {
            if (!first) os << ", ";
            first = false;
            os << scalarText(item);
        }
        os << "\n";
        return;
    }

    os << pad << key << ":\n";
    if (allObjects) {
        // Same `ls`-style table used for a top-level array, just indented -
        // this is what most "list" API responses actually look like: an
        // array of records nested under a key alongside other metadata.
        renderTable(os, arr, indent + 1);
        return;
    }

    int i = 0;
    std::string itemPad(static_cast<size_t>(indent + 1) * 2, ' ');
    for (JsonVariantConst item : arr) {
        os << itemPad << "- [" << i++ << "]\n";
        renderTree(os, item, indent + 2);
    }
}

void renderTree(std::ostream& os, JsonVariantConst value, int indent) {
    std::string pad(static_cast<size_t>(indent) * 2, ' ');
    if (value.is<JsonObjectConst>()) {
        for (JsonPairConst kv : value.as<JsonObjectConst>()) {
            std::string key = kv.key().c_str();
            JsonVariantConst v = kv.value();
            if (v.is<JsonObjectConst>()) {
                os << pad << key << ":\n";
                renderTree(os, v, indent + 1);
            } else if (v.is<JsonArrayConst>()) {
                renderTreeArrayValue(os, key, v.as<JsonArrayConst>(), indent);
            } else {
                os << pad << key << ": " << scalarText(v) << "\n";
            }
        }
    } else {
        os << pad << scalarText(value) << "\n";
    }
}

// Arrays of objects: an `ls -l`-style aligned table, one row per element,
// columns = the union of keys across all elements (in first-seen order).
void renderTable(std::ostream& os, JsonArrayConst arr, int indent) {
    std::string pad(static_cast<size_t>(indent) * 2, ' ');

    std::vector<std::string> columns;
    for (JsonObjectConst obj : arr) {
        for (JsonPairConst kv : obj) {
            std::string key = kv.key().c_str();
            if (std::find(columns.begin(), columns.end(), key) == columns.end()) columns.push_back(key);
        }
    }

    std::vector<std::vector<std::string>> rows;
    for (JsonObjectConst obj : arr) {
        std::vector<std::string> row;
        row.reserve(columns.size());
        for (const auto& col : columns) {
            JsonVariantConst v = obj[col.c_str()];
            row.push_back(v.isNull() ? "" : cellText(v));
        }
        rows.push_back(std::move(row));
    }

    std::vector<size_t> widths(columns.size());
    for (size_t c = 0; c < columns.size(); ++c) {
        widths[c] = columns[c].size();
        for (const auto& row : rows) widths[c] = std::max(widths[c], row[c].size());
    }

    os << pad;
    for (size_t c = 0; c < columns.size(); ++c) {
        os << std::left << std::setw(static_cast<int>(widths[c] + 2)) << columns[c];
    }
    os << "\n";
    for (const auto& row : rows) {
        os << pad;
        for (size_t c = 0; c < columns.size(); ++c) {
            os << std::left << std::setw(static_cast<int>(widths[c] + 2)) << row[c];
        }
        os << "\n";
    }
}

// Arrays of scalars: a terminal-width-aware multi-column grid, laid out
// top-to-bottom-then-next-column like plain `ls`. Falls back to one item
// per line when stdout isn't a terminal (also matching `ls`'s own behavior
// when piped/redirected).
void renderGrid(std::ostream& os, JsonArrayConst arr, int indent) {
    std::string pad(static_cast<size_t>(indent) * 2, ' ');

    std::vector<std::string> items;
    items.reserve(arr.size());
    for (JsonVariantConst v : arr) items.push_back(scalarText(v));

    if (!stdoutIsTerminal()) {
        for (const auto& s : items) os << pad << s << "\n";
        return;
    }

    size_t maxLen = 0;
    for (const auto& s : items) maxLen = std::max(maxLen, s.size());
    size_t colWidth = maxLen + 2;
    size_t available = terminalWidth() > pad.size() ? terminalWidth() - pad.size() : terminalWidth();
    size_t numCols = std::max<size_t>(1, available / std::max<size_t>(1, colWidth));
    size_t numRows = (items.size() + numCols - 1) / numCols;

    for (size_t r = 0; r < numRows; ++r) {
        os << pad;
        for (size_t c = 0; c < numCols; ++c) {
            size_t idx = c * numRows + r;
            if (idx < items.size()) {
                bool lastCol = (c + 1 == numCols) || (idx + numRows >= items.size());
                if (lastCol) {
                    os << items[idx];
                } else {
                    os << std::left << std::setw(static_cast<int>(colWidth)) << items[idx];
                }
            }
        }
        os << "\n";
    }
}

void renderList(std::ostream& os, JsonArrayConst arr) {
    if (arr.size() == 0) {
        os << "(empty)\n";
        return;
    }

    bool allObjects = true;
    bool allScalar = true;
    for (JsonVariantConst item : arr) {
        if (!item.is<JsonObjectConst>()) allObjects = false;
        if (item.is<JsonObjectConst>() || item.is<JsonArrayConst>()) allScalar = false;
    }

    if (allObjects) {
        renderTable(os, arr, 0);
    } else if (allScalar) {
        renderGrid(os, arr, 0);
    } else {
        int i = 0;
        for (JsonVariantConst item : arr) {
            os << "[" << i++ << "]\n";
            renderTree(os, item, 1);
        }
    }
}

}  // namespace

std::string formatHumanReadable(const std::string& text) {
    JsonDocument doc;
    if (deserializeJson(doc, text)) return text;  // not JSON (or malformed) - leave as-is

    std::ostringstream os;
    JsonVariantConst root = doc.as<JsonVariantConst>();
    if (root.is<JsonArrayConst>()) {
        renderList(os, root.as<JsonArrayConst>());
    } else if (root.is<JsonObjectConst>()) {
        renderTree(os, root, 0);
    } else {
        os << scalarText(root) << "\n";
    }
    return os.str();
}

}  // namespace commands
