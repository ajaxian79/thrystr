// SPDX-License-Identifier: LicenseRef-thrystr-dual
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct Page {
    std::filesystem::path path;
    std::string slug;
    std::string title;
    std::string markdown;
    std::string html;
};

struct Args {
    std::filesystem::path input_dir;
    std::filesystem::path site_dir;
    std::filesystem::path cpp_path;
    std::filesystem::path hpp_path;
};

std::string read_text(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("could not read " + path.string());
    }
    std::ostringstream out;
    out << input.rdbuf();
    return out.str();
}

void write_text(const std::filesystem::path& path, std::string_view text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path);
    if (!output) {
        throw std::runtime_error("could not write " + path.string());
    }
    output << text;
}

std::string trim(std::string_view text) {
    std::size_t first = 0;
    while (first < text.size() && std::isspace(static_cast<unsigned char>(text[first])) != 0) {
        ++first;
    }
    std::size_t last = text.size();
    while (last > first && std::isspace(static_cast<unsigned char>(text[last - 1u])) != 0) {
        --last;
    }
    return std::string(text.substr(first, last - first));
}

std::string escape_html(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (char ch : text) {
        switch (ch) {
        case '&':
            out += "&amp;";
            break;
        case '<':
            out += "&lt;";
            break;
        case '>':
            out += "&gt;";
            break;
        case '"':
            out += "&quot;";
            break;
        default:
            out.push_back(ch);
            break;
        }
    }
    return out;
}

std::string escape_json(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (char ch : text) {
        switch (ch) {
        case '\\':
            out += "\\\\";
            break;
        case '"':
            out += "\\\"";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            out.push_back(ch);
            break;
        }
    }
    return out;
}

std::string cpp_raw_string(std::string_view text) {
    std::string delimiter = "THRYSTR_DOC";
    while (text.find(")" + delimiter + "\"") != std::string_view::npos) {
        delimiter += "X";
    }
    return "R\"" + delimiter + "(" + std::string(text) + ")" + delimiter + "\"";
}

std::string slugify(std::string_view text) {
    std::string slug;
    bool dash = false;
    for (char raw : text) {
        const unsigned char ch = static_cast<unsigned char>(raw);
        if (std::isalnum(ch) != 0) {
            if (dash && !slug.empty()) {
                slug.push_back('-');
            }
            slug.push_back(static_cast<char>(std::tolower(ch)));
            dash = false;
        } else if (!slug.empty()) {
            dash = true;
        }
    }
    return slug.empty() ? "page" : slug;
}

std::vector<std::string> split_lines(std::string_view text) {
    std::vector<std::string> lines;
    std::string current;
    for (char ch : text) {
        if (ch == '\n') {
            if (!current.empty() && current.back() == '\r') {
                current.pop_back();
            }
            lines.push_back(current);
            current.clear();
        } else {
            current.push_back(ch);
        }
    }
    if (!current.empty()) {
        lines.push_back(current);
    }
    return lines;
}

std::string inline_markup(std::string_view text) {
    std::string out;
    bool code = false;
    std::string buffer;
    for (char ch : text) {
        if (ch == '`') {
            out += code ? escape_html(buffer) + "</code>" : escape_html(buffer) + "<code>";
            buffer.clear();
            code = !code;
        } else {
            buffer.push_back(ch);
        }
    }
    out += escape_html(buffer);
    if (code) {
        out += "</code>";
    }
    return out;
}

std::string render_markdown(std::string_view markdown) {
    const std::vector<std::string> lines = split_lines(markdown);
    std::ostringstream html;
    bool in_paragraph = false;
    bool in_ul = false;
    bool in_ol = false;
    bool in_code = false;

    const auto close_paragraph = [&]() {
        if (in_paragraph) {
            html << "</p>\n";
            in_paragraph = false;
        }
    };
    const auto close_lists = [&]() {
        if (in_ul) {
            html << "</ul>\n";
            in_ul = false;
        }
        if (in_ol) {
            html << "</ol>\n";
            in_ol = false;
        }
    };

    for (const std::string& raw_line : lines) {
        const std::string line = trim(raw_line);
        if (line.rfind("```", 0) == 0) {
            close_paragraph();
            close_lists();
            if (!in_code) {
                html << "<pre><code>";
            } else {
                html << "</code></pre>\n";
            }
            in_code = !in_code;
            continue;
        }
        if (in_code) {
            html << escape_html(raw_line) << '\n';
            continue;
        }
        if (line.empty()) {
            close_paragraph();
            close_lists();
            continue;
        }
        if (line.rfind("# ", 0) == 0 || line.rfind("## ", 0) == 0 || line.rfind("### ", 0) == 0) {
            close_paragraph();
            close_lists();
            const int level = line.rfind("### ", 0) == 0 ? 3 : line.rfind("## ", 0) == 0 ? 2 : 1;
            const std::string text =
                trim(std::string_view(line).substr(static_cast<std::size_t>(level) + 1u));
            html << "<h" << level << " id=\"" << slugify(text) << "\">" << inline_markup(text)
                 << "</h" << level << ">\n";
            continue;
        }
        if (line.rfind("- ", 0) == 0) {
            close_paragraph();
            if (!in_ul) {
                close_lists();
                html << "<ul>\n";
                in_ul = true;
            }
            html << "<li>" << inline_markup(std::string_view(line).substr(2)) << "</li>\n";
            continue;
        }
        if (line.size() > 3 && std::isdigit(static_cast<unsigned char>(line[0])) != 0 &&
            line[1] == '.' && line[2] == ' ') {
            close_paragraph();
            if (!in_ol) {
                close_lists();
                html << "<ol>\n";
                in_ol = true;
            }
            html << "<li>" << inline_markup(std::string_view(line).substr(3)) << "</li>\n";
            continue;
        }
        close_lists();
        if (!in_paragraph) {
            html << "<p>";
            in_paragraph = true;
        } else {
            html << ' ';
        }
        html << inline_markup(line);
    }
    close_paragraph();
    close_lists();
    if (in_code) {
        html << "</code></pre>\n";
    }
    return html.str();
}

std::string page_title(std::string_view markdown, const std::filesystem::path& path) {
    for (const std::string& line : split_lines(markdown)) {
        if (line.rfind("# ", 0) == 0) {
            return trim(std::string_view(line).substr(2));
        }
    }
    return path.stem().string();
}

std::vector<Page> load_pages(const std::filesystem::path& input_dir) {
    std::vector<std::filesystem::path> paths;
    for (const std::filesystem::directory_entry& entry :
         std::filesystem::directory_iterator(input_dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".md") {
            paths.push_back(entry.path());
        }
    }
    std::sort(paths.begin(), paths.end(), [](const auto& left, const auto& right) {
        const bool left_index = left.filename() == "index.md";
        const bool right_index = right.filename() == "index.md";
        if (left_index != right_index) {
            return left_index;
        }
        return left < right;
    });

    std::set<std::string> used_slugs;
    std::vector<Page> pages;
    for (const std::filesystem::path& path : paths) {
        Page page;
        page.path = path;
        page.markdown = read_text(path);
        page.title = page_title(page.markdown, path);
        page.slug = slugify(path.stem().string());
        std::string unique = page.slug;
        int suffix = 2;
        while (!used_slugs.insert(unique).second) {
            unique = page.slug + "-" + std::to_string(suffix++);
        }
        page.slug = unique;
        page.html = render_markdown(page.markdown);
        pages.push_back(std::move(page));
    }
    return pages;
}

std::vector<std::string> tokens_for(std::string_view text) {
    std::set<std::string> tokens;
    std::string current;
    for (char raw : text) {
        const unsigned char ch = static_cast<unsigned char>(raw);
        if (std::isalnum(ch) != 0) {
            current.push_back(static_cast<char>(std::tolower(ch)));
        } else if (!current.empty()) {
            if (current.size() >= 2u) {
                tokens.insert(current);
            }
            current.clear();
        }
    }
    if (current.size() >= 2u) {
        tokens.insert(current);
    }
    return {tokens.begin(), tokens.end()};
}

std::string site_shell(const std::vector<Page>& pages, const Page& current) {
    std::ostringstream out;
    out << "<!doctype html><html><head><meta charset=\"utf-8\">"
        << "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
        << "<title>" << escape_html(current.title) << " - thrystr</title>"
        << "<link rel=\"stylesheet\" href=\"style.css\"></head><body><nav><strong>thrystr</strong>";
    for (const Page& page : pages) {
        out << "<a href=\"" << page.slug << ".html\""
            << (page.slug == current.slug ? " class=\"active\"" : "") << ">"
            << escape_html(page.title) << "</a>";
    }
    out << "</nav><main><input id=\"search\" placeholder=\"Search manual\" autocomplete=\"off\">"
        << "<div id=\"results\"></div><article>" << current.html
        << "</article></main><script src=\"search.js\"></script></body></html>\n";
    return out.str();
}

void emit_site(const std::vector<Page>& pages, const std::filesystem::path& site_dir) {
    std::filesystem::create_directories(site_dir);
    write_text(
        site_dir / "style.css",
        "body{margin:0;background:#171512;color:#ece6d8;font:15px Inter,system-ui,sans-serif;}"
        "nav{position:fixed;inset:0 auto 0 "
        "0;width:260px;overflow:auto;background:#211e1a;padding:18px;}"
        "nav strong{display:block;font-size:22px;margin-bottom:14px;color:#7ddbd4;}"
        "nav a{display:block;color:#bdb5a8;text-decoration:none;padding:7px 0;}"
        "nav a.active,nav a:hover{color:#ece6d8;}"
        "main{margin-left:296px;max-width:920px;padding:28px 44px;}"
        "article{line-height:1.62;}"
        "h1,h2,h3{color:#f7f0df;letter-spacing:0;}"
        "code,pre{background:#2a261f;color:#f4d58d;border-radius:4px;}"
        "code{padding:1px 4px;}pre{padding:14px;overflow:auto;}"
        "#search{width:100%;box-sizing:border-box;margin-bottom:14px;background:#24211c;color:#"
        "ece6d8;border:1px solid #4b463d;padding:10px;}"
        "#results a{display:block;color:#7ddbd4;text-decoration:none;padding:4px 0;}"
        "@media(max-width:760px){nav{position:static;width:auto;}main{margin-left:0;padding:22px;}}"
        "\n");

    std::ostringstream index;
    index << "const THRYSTR_SEARCH_INDEX=[";
    for (std::size_t i = 0; i < pages.size(); ++i) {
        if (i > 0) {
            index << ',';
        }
        index << "{\"title\":\"" << escape_json(pages[i].title) << "\",\"url\":\"" << pages[i].slug
              << ".html\",\"tokens\":[";
        const std::vector<std::string> tokens =
            tokens_for(pages[i].title + "\n" + pages[i].markdown);
        for (std::size_t token = 0; token < tokens.size(); ++token) {
            if (token > 0) {
                index << ',';
            }
            index << "\"" << escape_json(tokens[token]) << "\"";
        }
        index << "]}";
    }
    index
        << "];\n"
        << "const box=document.getElementById('search');const "
           "results=document.getElementById('results');"
        << "box.addEventListener('input',()=>{const "
           "q=box.value.toLowerCase().split(/\\W+/).filter(Boolean);"
        << "results.innerHTML='';if(!q.length)return;for(const p of THRYSTR_SEARCH_INDEX){"
        << "if(q.every(t=>p.tokens.includes(t))){const a=document.createElement('a');a.href=p.url;"
        << "a.textContent=p.title;results.appendChild(a);}}});\n";
    write_text(site_dir / "search.js", index.str());

    for (const Page& page : pages) {
        write_text(site_dir / (page.slug + ".html"), site_shell(pages, page));
    }
    if (!pages.empty()) {
        write_text(site_dir / "index.html", site_shell(pages, pages.front()));
    }
}

void emit_cpp_resources(const std::vector<Page>& pages, const std::filesystem::path& cpp_path,
                        const std::filesystem::path& hpp_path) {
    write_text(hpp_path, "#pragma once\n"
                         "#include <cstddef>\n"
                         "#include <string_view>\n\n"
                         "namespace thrystr::docs {\n"
                         "struct DocPage { std::string_view slug; std::string_view title; "
                         "std::string_view markdown; std::string_view html; };\n"
                         "std::size_t doc_page_count();\n"
                         "const DocPage* doc_pages();\n"
                         "}  // namespace thrystr::docs\n");

    std::ostringstream cpp;
    cpp << "// !!! GENERATED BY tools/docgen - DO NOT EDIT BY HAND !!!\n"
        << "#include \"docs_resources.hpp\"\n\n"
        << "#include <array>\n\n"
        << "namespace thrystr::docs {\nnamespace {\n"
        << "constexpr std::array<DocPage, " << pages.size() << "> kPages = {{\n";
    for (const Page& page : pages) {
        cpp << "    {\"" << escape_json(page.slug) << "\", \"" << escape_json(page.title) << "\", "
            << cpp_raw_string(page.markdown) << ", " << cpp_raw_string(page.html) << "},\n";
    }
    cpp << "}};\n}  // namespace\n\n"
        << "std::size_t doc_page_count() { return kPages.size(); }\n"
        << "const DocPage* doc_pages() { return kPages.data(); }\n"
        << "}  // namespace thrystr::docs\n";
    write_text(cpp_path, cpp.str());
}

Args parse_args(int argc, char** argv) {
    Args args;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const auto read_next = [&](std::filesystem::path& out, const char* name) {
            if (arg == name && i + 1 < argc) {
                out = argv[++i];
                return true;
            }
            return false;
        };
        read_next(args.input_dir, "--input");
        read_next(args.site_dir, "--site");
        read_next(args.cpp_path, "--cpp");
        read_next(args.hpp_path, "--hpp");
    }
    if (args.input_dir.empty() || args.site_dir.empty() || args.cpp_path.empty() ||
        args.hpp_path.empty()) {
        throw std::runtime_error("usage: thrystr_docgen --input docs/user --site build/docs/site "
                                 "--cpp build/generated/docs/docs_resources.cpp "
                                 "--hpp build/generated/docs/docs_resources.hpp");
    }
    return args;
}

} // namespace

int main(int argc, char** argv) {
    try {
        const Args args = parse_args(argc, argv);
        const std::vector<Page> pages = load_pages(args.input_dir);
        emit_site(pages, args.site_dir);
        emit_cpp_resources(pages, args.cpp_path, args.hpp_path);
    } catch (const std::exception& error) {
        std::cerr << "thrystr_docgen: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
