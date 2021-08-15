#pragma once
#include <iostream>
#include <string>
#include <string_view>
#include "nwg_classes.h"
#include "filesystem-compat.h"

namespace OnDesktopEntry {
    struct Ok{}     Ok_;
    struct Error{}  Error_;
    struct Hidden{} Hidden_;
}

/* Stores pre-processed assets useful when parsing DesktopEntry struct */
struct DesktopEntryConfig {
    std::string_view term;  // user-preferred terminal
    std::string name_ln;    // localized prefix: Name[ln]=
    std::string comment_ln; // localized prefix: Comment[ln]=

    DesktopEntryConfig(std::string_view lang, std::string_view term):
        term{ term },
        name_ln{ concat("Name[", lang, "]=") },
        comment_ln{ concat("Comment[", lang, "]=") }
    {
        // intentionally left blank
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

    DesktopEntry entry;
    entry.terminal = false;

    std::ifstream file{ path };
    if (!file) {
        f(OnDesktopEntry::Error_);
        return;
    }
    std::string str; // buffer to read into

    std::string name_ln {};    // localized: Name[ln]=
    std::string comment_ln {}; // localized: Comment[ln]=

    // action to perform on value before writing it to the dest
    struct nop_t { } nop;
    struct cut_t { } cut;
    // if line starts with `prefix`, process the rest of the line according to `tag`
    // writing the result to `dest`
    struct Match {
        std::string_view           prefix;
        std::string*               dest;   // non-null
        std::variant<nop_t, cut_t> tag;
    };
    struct Result {
        bool   ok;
        size_t pos;
    };
    Match matches[] = {
        { "Name="sv,         &entry.name,      nop },
        { config.name_ln,    &name_ln,         nop },
        { "Exec="sv,         &entry.exec,      cut },
        { "Icon="sv,         &entry.icon,      nop },
        { "Comment="sv,      &entry.comment,   nop },
        { config.comment_ln, &comment_ln,      nop },
        { "MimeType="sv,     &entry.mime_type, nop },
    };

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
    while (std::getline(file, str)) {
        if (str[0] == '[') { // new section begins, break
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
        for (auto& [prefix, dest, tag] : matches) {
            if (auto [ok, pos] = try_strip_prefix(prefix); ok) {
                std::visit(Overloaded {
                    [dest=dest, pos=pos, &view](nop_t) { *dest = view.substr(pos); },
                    [dest=dest, pos=pos, &view](cut_t) {
                        auto idx = view.find(" %", pos);
                        if (idx == std::string_view::npos) {
                            idx = std::size(view);
                        }
                        *dest = view.substr(pos, idx - pos);
                    }
                },
                tag);
                break;
            }
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
    f(OnDesktopEntry::Ok_, std::move(entry));
}
