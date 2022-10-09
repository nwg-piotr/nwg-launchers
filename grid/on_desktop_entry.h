#pragma once

#ifndef ON_DESKTOP_ENTRY_H
#define ON_DESKTOP_ENTRY_H

#include <bitset>
#include <iostream>
#include <fstream>
#include <string>
#include <string_view>
#include "nwg_classes.h"
#include "nwg_tools.h"
#include "filesystem-compat.h"

namespace OnDesktopEntry {
    inline struct Error{}  Error_;
    inline struct Hidden{} Hidden_;
}

/* Stores pre-processed assets useful when parsing DesktopEntry struct */
struct DesktopEntryConfig {
    std::string_view term;  // user-preferred terminal
    std::string name_ln;    // localized prefix: Name[ln]=
    std::string comment_ln; // localized prefix: Comment[ln]=
    std::string_view home;
    const std::vector<std::string_view>& known_categories;

    DesktopEntryConfig(std::string_view lang, std::string_view term):
        term{ term },
        name_ln{ concat("Name[", lang, "]=") },
        comment_ln{ concat("Comment[", lang, "]=") },
        home{ get_home_dir() },
        known_categories{ category::get_known_categories("nwggrid") }
    {
        // intentionally left blank
    }
};

struct FieldParser {
    virtual ~FieldParser() = default;
    virtual void parse(std::string_view) = 0;
};

struct PlainFieldParser: public FieldParser {
    std::string& dest;

    PlainFieldParser(std::string& dest): dest{ dest } {
        // intentionally left blank
    }
    virtual ~PlainFieldParser() = default;
    virtual void parse(std::string_view str) override {
        dest = str;
    }
};

struct ExecParser: public PlainFieldParser {
    using PlainFieldParser::PlainFieldParser;
    void parse(std::string_view str) override {
        std::string_view home{ "~/" };
        if (str.substr(0, home.size()) == home) {
            str.remove_prefix(home.size());
            dest += get_home_dir();
            dest.push_back('/');
        }

        auto idx = str.find(" %");
        if (idx == std::string_view::npos) {
            idx = std::size(str);
        }
        dest += str.substr(0, idx);
    }
};

struct CategoryParser: public FieldParser {
    using CategoriesType = decltype(DesktopEntry{}.categories);
    CategoriesType& categories;
    const std::vector<std::string_view>& known_categories;

    CategoryParser(CategoriesType& categories, const std::vector<std::string_view>& known_categories):
        categories{ categories },
        known_categories{ known_categories }
    {
        // intentionally left blank
    }

    void parse(std::string_view str) override {
        auto is_known_category = [&](auto && c){
            return std::find(known_categories.begin(), known_categories.end(), c) != known_categories.end();
        };
        auto parts = split_string(str, ";");
        for (auto && part: parts) {
            if (!part.empty() && is_known_category(part)) {
                categories.emplace_back(part);
            }
        }
    }
};

/*
 * Parses .desktop file to DesktopEntry struct, calling visitor `f` with respective type tags
 *  - f(OnDesktopEntry::Ok, DesktopEntry &&)
 *  - f(OnDesktopEntry::Hidden)
 *  - f(OnDesktopEntry::Error)
 * so `f` should have listed `operator()` overloads.
 * Advice: use Overloaded+lambdas to create a visitor rather than writing one by hand.
 * */
template <typename F>
void on_desktop_entry(const fs::path& path, const DesktopEntryConfig& config, F && f) {
    using namespace std::literals::string_view_literals;

    std::unique_ptr<DesktopEntry> entry_ptr{ new DesktopEntry{} };
    auto & entry = *entry_ptr;
    //DesktopEntry entry;
    entry.terminal = false;

    std::ifstream file{ path };
    if (!file) {
        f(OnDesktopEntry::Error_);
        return;
    }
    std::string str; // buffer to read into

    std::string name_ln {};    // localized: Name[ln]=
    std::string comment_ln {}; // localized: Comment[ln]=

    // if line starts with `prefix`, call `parser.parse()`
    struct Match {
        std::string_view prefix;
        FieldParser&     parser;
    };
    struct Result {
        bool   ok;
        size_t pos;
    };
    PlainFieldParser name_parser{ entry.name };
    PlainFieldParser name_ln_parser{ name_ln };
    ExecParser exec_parser{ entry.exec };
    PlainFieldParser icon_parser{ entry.icon };
    PlainFieldParser comment_parser{ entry.comment };
    PlainFieldParser comment_ln_parser{ comment_ln };
    PlainFieldParser mime_type_parser{ entry.mime_type };
    CategoryParser categories_parser{ entry.categories, config.known_categories };

    Match matches[] = {
        { "Name="sv,         name_parser },
        { config.name_ln,    name_ln_parser },
        { "Exec="sv,         exec_parser },
        { "Icon="sv,         icon_parser },
        { "Comment="sv,      comment_parser },
        { config.comment_ln, comment_ln_parser },
        { "MimeType="sv,     mime_type_parser },
        { "Categories="sv,   categories_parser }
    };
    std::bitset<std::size(matches)> parsed;

    // Skip everything not related
    constexpr auto header = "[Desktop Entry]"sv;
    while (std::getline(file, str)) {
        str.resize(header.size());
        if (str == header) {
            break;
        }
    }
    // Repeat until the next section
    constexpr auto nodisplay = "NoDisplay=true"sv;
    constexpr auto terminal = "Terminal=true"sv;
    while (std::getline(file, str) && !parsed.all()) {
        if (!str.empty() && str[0] == '[') { // new section begins, break
            break;
        }
        auto view = std::string_view{str};
        auto view_len = std::size(view);
        if (view == nodisplay) {
            f(OnDesktopEntry::Hidden_);
            return;
        }
        if (view == terminal) {
            entry.terminal = true;
        }
        auto try_strip_prefix = [&view, view_len](auto& prefix) {
            auto len = std::min(view_len, std::size(prefix));
            return Result {
                prefix == view.substr(0, len),
                len
            };
        };
        std::size_t index{ 0 };
        for (auto& [prefix, parser] : matches) {
            if (parsed[index]) {
                continue;
            }
            if (auto [ok, pos] = try_strip_prefix(prefix); ok) {
                parser.parse(view.substr(pos));
                break;
            }
            ++index;
        }
    }

    if (!name_ln.empty()) {
        entry.name = std::move(name_ln);
    }
    if (!comment_ln.empty()) {
        entry.comment = std::move(comment_ln);
    }
    if (entry.name.empty() || entry.exec.empty()) {
        f(OnDesktopEntry::Error_);
    }
    if (entry.terminal) {
        entry.exec = concat(config.term, " ", entry.exec);
    }
    f(std::move(entry_ptr));
}

#endif
