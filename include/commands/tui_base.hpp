#pragma once

#include <ftxui/component/component_base.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <string>
#include <vector>

class TuiBase : public ftxui::ComponentBase {
protected:
    int selected_ = 0;
    int scroll_offset_ = 0;
    int max_rows_ = 0;
    std::string search_query_;
    std::string search_input_;
    bool search_mode_ = false;

    virtual int entries_size() const = 0;
    virtual ftxui::Element render_row(int idx) const = 0;
    virtual void fill_entries() = 0;
    virtual int header_rows() const = 0;
    virtual bool on_command_key(ftxui::Event event) { return false; }
    virtual void on_search_input_changed() {}

    void update_scroll_math();
    ftxui::Element render_list() const;
    ftxui::Element render_search_bar() const;
    bool handle_nav(ftxui::Event event);
    bool handle_search(ftxui::Event event);

public:
    void Refresh();
};
