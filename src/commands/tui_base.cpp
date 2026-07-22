#include "commands/tui_base.hpp"

#include <ftxui/component/app.hpp>

void TuiBase::update_scroll_math() {
    if (auto* app = ftxui::App::Active()) {
        int h = app->dimy();
        int header_rows = 1;
        max_rows_ = h - header_rows - 2;
        if (max_rows_ < 1) max_rows_ = 1;
    }

    int total = entries_size();
    int scroll_max = total - max_rows_;
    if (scroll_max < 0) scroll_max = 0;
    if (scroll_offset_ > scroll_max) scroll_offset_ = scroll_max;
    if (selected_ >= total) selected_ = total - 1;
    if (selected_ < 0) selected_ = 0;
}

ftxui::Element TuiBase::render_list() const {
    using namespace ftxui;

    Elements rows;
    int display_count = entries_size();
    int avail = max_rows_;
    if (display_count > avail) display_count = avail;

    for (int i = 0; i < display_count; i++) {
        int idx = i + scroll_offset_;
        if (idx >= entries_size()) break;

        auto el = render_row(idx);
        if (i == selected_ - scroll_offset_) {
            el = el | inverted;
        } else if (i % 2 == 1) {
            el = el | bgcolor(Color::GrayDark);
        }
        rows.push_back(el);
    }

    if (rows.empty()) {
        rows.push_back(text("(empty)") | dim | center);
    }

    return vbox(rows) | vscroll_indicator | yframe | flex;
}

ftxui::Element TuiBase::render_search_bar() const {
    using namespace ftxui;

    if (search_mode_) {
        return hbox({
            text("Search: ") | bold | color(Color::Yellow),
            text(search_input_) | color(Color::White),
            text("_") | blink | color(Color::White),
        }) | frame;
    } else if (!search_query_.empty()) {
        return hbox({
            text("Filter: ") | bold | color(Color::Yellow),
            text(search_query_) | color(Color::Cyan),
            text("  (press / to change, Esc to clear)") | color(Color::GrayDark),
        }) | frame;
    }
    return text("Press / to search") | color(Color::GrayDark) | frame;
}

bool TuiBase::handle_nav(ftxui::Event event) {
    using namespace ftxui;

    if (event == Event::ArrowUp || event == Event::Character('k')) {
        if (selected_ > 0) selected_--;
        update_scroll_math();
        return true;
    }
    if (event == Event::ArrowDown || event == Event::Character('j')) {
        if (selected_ < entries_size() - 1) selected_++;
        update_scroll_math();
        return true;
    }
    if (event == Event::PageDown) {
        selected_ += max_rows_ / 2;
        if (selected_ >= entries_size()) selected_ = entries_size() - 1;
        update_scroll_math();
        return true;
    }
    if (event == Event::PageUp) {
        selected_ -= max_rows_ / 2;
        if (selected_ < 0) selected_ = 0;
        update_scroll_math();
        return true;
    }
    if (event == Event::Home) {
        selected_ = 0;
        scroll_offset_ = 0;
        return true;
    }
    if (event == Event::End) {
        selected_ = entries_size() - 1;
        if (selected_ < 0) selected_ = 0;
        update_scroll_math();
        return true;
    }
    return false;
}

bool TuiBase::handle_search(ftxui::Event event) {
    using namespace ftxui;

    if (!search_mode_) return false;

    if (event == Event::Backspace) {
        if (!search_input_.empty()) {
            search_input_.pop_back();
        }
        on_search_input_changed();
        return true;
    }
    if (event.is_character()) {
        std::string ch = event.character();
        if (!ch.empty() && (unsigned char)ch[0] >= 32 && (unsigned char)ch[0] < 127) {
            search_input_ += ch[0];
        }
        on_search_input_changed();
        return true;
    }
    return false;
}

void TuiBase::Refresh() {
    fill_entries();
    update_scroll_math();
}
