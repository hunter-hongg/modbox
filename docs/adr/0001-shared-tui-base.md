# Shared TUI Base Class for modbox interactive commands

Currently `ls_tui` and `ps_tui` each carry their own copy of the same scaffolding: `scroll_offset_`, `max_rows_`, `selected_idx_`, search input/query state, keyboard event dispatch (j/k nav, `/` search, `q` quit), row rendering with alternating background, and a header/footer layout strip. `ps_tui` additionally owns an async refresher thread that posts `Event::Custom` to drive periodic redraws.

Every new TUI command (`cat --tui`, `find --tui`, etc.) would copy-paste this scaffolding again. We're building a suite of "BusyBox for the 2025 terminal" commands, so the duplication cost compounds fast.

We decided to extract a `TuiBase` abstract component class that owns all shared state and behaviour. Each concrete command (`LsfComponent`, `PsTuiComponent`) overrides three virtual hooks: `fill_entries()` to populate its data vector, `render_row(idx)` to produce one row element, and `on_key(event)` for command-specific bindings. The base class handles scroll math, search/filter, nav keys, and the header/footer chrome.

## Considered Options

- **Copy-paste per command (status quo).** Zero upfront cost, but every new TUI command triples the maintenance surface. Scroll behaviour, search UX, and footer keybindings drift apart over time — already visible: `ls_tui` lacks `ps_tui`'s pagination and live refresh.
- **Composition over inheritance (mix-in struct).** `TuiState` struct embedded in each component. Works, but ftxui's `ComponentBase::OnEvent` is a single virtual dispatch; splitting state from behaviour across two objects makes the event loop awkward.
- **Shared template base (`TuiBase<T>`).** In principle cleaner, but adds generic complexity for modest gain; the three hooks are stable enough that a non-template abstract base is simpler.

## Consequences

- All future TUI commands inherit consistent UX (scroll, search, keybindings) without re-implementation.
- `ls_tui` will piggy-back on the base-class scroll/subject infrastructure, fixing the unbounded-render loop on large directories.
- `ps_tui`'s refresher thread moves to the base class (configurable interval per subclass), so any command can opt into live polling (e.g. `cat --tui` watching a log file).
