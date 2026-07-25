# 01 — Add alternating row backgrounds to ls --tui

**What to build:** When running `ls --tui` or `lf`, the file list shows subtle zebra-striping so users can track their place across long listings. Even-indexed rows keep the default background; odd-indexed rows get a low-contrast dim background. When color is disabled, no background is applied.

**Blocked by:** None — can start immediately.

**Status:** ready-for-agent

- [ ] `LsfComponent::render_row(int idx)` applies a background color to odd-indexed rows when color is active
- [ ] Even-indexed rows remain unchanged (no background)
- [ ] When color is disabled, all rows render with no background (ANSI-free output)
- [ ] The alternating pattern follows the visible row index, so filtering and sorting produce stable visuals
- [ ] No new state is added to `LsfComponent`; parity is computed inline from `idx`
- [ ] `TuiBase` interface remains unchanged
- [ ] Manual verification: `modbox ls --tui .` shows alternating rows; `modbox ls --tui --color=never .` shows no backgrounds
