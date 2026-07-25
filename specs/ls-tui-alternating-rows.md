# Spec: Alternating Row Background Colors for ls --tui

## Problem Statement

When browsing large directories in `ls --tui` (or `lf`), every row is rendered as plain text on the same background. The eye has no horizontal anchor to track which line it's on, so scanning a long list requires re-reading the filename on every keystroke. Users accustomed to ls/lsd/exa expect subtle zebra-striping to make list scanning faster and reduce cognitive load.

## Solution

Add an alternating row background color to `LsfComponent::render_row()`. Even-indexed rows keep the default background; odd-indexed rows get a subtle dim background. The color choice respects the existing `--color` / `--colorful` conventions: when color is disabled, no background is applied; when enabled, a low-contrast bg is used that doesn't fight with the existing foreground file-type icons.

## User Stories

1. As a user browsing a directory with many entries, I want every other row to have a slightly different background, so that I can track my place across long listings.
2. As a user with `--colorful` enabled, I want the alternating rows to use a subtle background tint, so that the zebra striping doesn't overwhelm the existing file-type icons and permission colors.
3. As a user with color disabled (e.g. piped output, `--color=never`), I want no background color applied, so that output stays clean and ANSI-free.
4. As a user scrolling through a filtered list after pressing `/`, I want the alternating pattern to remain stable across the filtered subset, so that the visual rhythm doesn't jump around.
5. As a user, I want the alternating pattern to be based on the visible row index rather than the underlying data index, so that filtering and sorting don't produce visual glitches.

## Implementation Decisions

- **Render-time index parity.** `render_row(int idx)` already receives the visible row index from `TuiBase::render_list()`. The alternating color is determined by `idx % 2`, applied as an ftxui `bgcolor` or `bg` element decorator. This keeps the change local to one method.
- **Color choice.** When color is active, even rows: default background; odd rows: `Color::GrayDark` at low opacity or `bgcolor(Color::GrayDark)` (ftxui native). This matches the existing `dim` and `color` vocabulary already used in `OnRender()`. When color is inactive, the method returns plain `text(...)` with no background, preserving the current behavior.
- **No new state.** `LsfComponent` does not need a new member. The parity is computed inline from the `idx` parameter, avoiding any history-stack or sort-mode interaction.
- **TuiBase contract unchanged.** `TuiBase::render_row()` remains a pure virtual returning `ftxui::Element`. No base-class changes are required; `PsTuiComponent` and any future TUI can adopt the same pattern independently.

## Testing Decisions

- **Highest seam:** `render_row()` output under simulated ftxui rendering. Test by constructing an `LsfComponent`, calling `fill_entries()` with a fixed directory, then asserting that even/odd indices produce Elements with different background decorations.
- **Color-gated behavior:** One test with color forced on, one with color forced off, verifying that background-only appears in the color-on case.
- **Filter stability:** Apply a search query, verify that the alternating pattern still follows visible row index parity (first visible row = even background).

## Out of Scope

- Alternating row colors in `ps --tui` (`PsTuiComponent`). That command has its own `render_row` and is deferred to a future spec.
- Syntax-highlighted preview pane (bat/delta style).
- Tree view indentation for directories.
- Multi-select / bulk operations.
- Remote filesystem support.

## Further Notes

- ADR 0001 (shared TUI base class) already constrains this: the change must stay in the subclass `render_row()` override and must not require base-class modifications.
- The ftxui `bgcolor` and `bg` decorators are the same primitives already used in `OnRender()` for header/footer styling, so no new library dependency is introduced.
