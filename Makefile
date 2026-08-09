# modbox — pure Makefile build (no CMake)
#
# Features:
#   * Recursive automatic source discovery (src/**/*.cpp)
#   * Per-object .d dependency files (-MMD -MP) so header changes
#     trigger smart, granular rebuilds of only affected translation units
#   * Generated compile_commands.json so clang-tidy works out of the box
#   * pkg-config driven dependency resolution (argtable3, ftxui, openssl)
#
# Targets:
#   make            / make compile   build target/modbox
#   make run                           build then run ./target/modbox
#   make lint                         run clang-tidy over all sources
#   make clean                        remove build/, target/, compile_commands.json
#   make refresh                      clean + full rebuild
#   make refresh-tidy                 clean + full rebuild with clang-tidy
#   make man                          generate man pages from markdown
#   make install-man                  install man pages to $(PREFIX)/share/man/man1/
#   make uninstall-man                remove installed man pages
#
# Options:
#   DEBUG=1         add -g -O0
#   TIDY=1          run clang-tidy during compilation (build-time analysis)
#   CXX=clang++     select compiler
#   PREFIX=/usr/local  installation prefix
#   DESTDIR=         staging directory for packaging

# --------------------------------------------------------------------------
# Directories & target
# --------------------------------------------------------------------------
SRC_DIR    := src
INC_DIR    := include
BUILD_DIR  := build
TARGET_DIR := target
TARGET     := $(TARGET_DIR)/modbox

# --------------------------------------------------------------------------
# Toolchain
# --------------------------------------------------------------------------
CXX    ?= g++
CXXSTD := c++20

ifeq ($(DEBUG),1)
CXXFLAGS += -g -O0
endif

# --------------------------------------------------------------------------
# Automatic recursive source discovery
# --------------------------------------------------------------------------
SRC := $(shell find $(SRC_DIR) -name '*.cpp' -type f | LC_ALL=C sort)
OBJ := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SRC))
DEP := $(OBJ:.o=.d)

# --------------------------------------------------------------------------
# Dependencies (pkg-config)
# --------------------------------------------------------------------------
# Support linuxbrew pkg-config path (can be overridden via environment)
LINUXBREW_PKGCONFIG ?= /home/linuxbrew/.linuxbrew/lib/pkgconfig
PKG_CONFIG_PATH := $(LINUXBREW_PKGCONFIG):$(PKG_CONFIG_PATH)
PKGS := argtable3 ftxui openssl libselinux libacl

PKG_CFLAGS := $(shell PKG_CONFIG_PATH="$(PKG_CONFIG_PATH)" pkg-config --cflags $(PKGS))
PKG_LIBS   := $(shell PKG_CONFIG_PATH="$(PKG_CONFIG_PATH)" pkg-config --libs   $(PKGS))

# --------------------------------------------------------------------------
# Compile / link flags
#   -MMD -MP : emit a .d dependency file alongside every .o (smart rebuilds)
# --------------------------------------------------------------------------
CPPFLAGS := -I$(INC_DIR) $(PKG_CFLAGS)
CXXFLAGS += -std=$(CXXSTD) -MMD -MP

# CMake auto-injects an RPATH for every linked library into the build-tree
# binary, which is what lets the executable find linuxbrew .so files at
# runtime. Replicate that here from the -L flags pkg-config reported.
RPATH_LIBDIRS := $(filter -L%,$(PKG_LIBS))
RPATH_FLAGS   := $(RPATH_LIBDIRS:-L%=-Wl,-rpath,%)

LDLIBS   := $(PKG_LIBS) $(RPATH_FLAGS) -lstdc++fs

# clang-tidy location (may be empty)
CLANG_TIDY := $(shell command -v clang-tidy 2>/dev/null)

# pandoc location (for man page generation)
PANDOC := $(shell command -v pandoc 2>/dev/null)

# Installation prefix
PREFIX ?= /usr/local
DESTDIR ?=

# Man page directories
MAN_SRC_DIR := docs/man
MAN_BUILD_DIR := $(BUILD_DIR)/man
MAN_INSTALL_DIR := $(DESTDIR)$(PREFIX)/share/man/man1

# Man page sources and targets
MAN_SOURCES := $(MAN_SRC_DIR)/modbox-cat.1.md \
                  $(MAN_SRC_DIR)/modbox-ls.1.md \
                  $(MAN_SRC_DIR)/modbox-rm.1.md \
                  $(MAN_SRC_DIR)/modbox-cp.1.md \
                  $(MAN_SRC_DIR)/modbox-mv.1.md \
                  $(MAN_SRC_DIR)/modbox-rmdir.1.md \
                  $(MAN_SRC_DIR)/modbox-arch.1.md \
                  $(MAN_SRC_DIR)/modbox-audit2allow.1.md \
                  $(MAN_SRC_DIR)/modbox-awk.1.md \
                  $(MAN_SRC_DIR)/modbox-head.1.md \
                  $(MAN_SRC_DIR)/modbox-tail.1.md \
                  $(MAN_SRC_DIR)/modbox-sort.1.md \
                  $(MAN_SRC_DIR)/modbox-grep.1.md \
                  $(MAN_SRC_DIR)/modbox-sed.1.md \
                  $(MAN_SRC_DIR)/modbox-find.1.md \
                  $(MAN_SRC_DIR)/modbox-stat.1.md \
                  $(MAN_SRC_DIR)/modbox-diff.1.md \
                  $(MAN_SRC_DIR)/modbox-cut.1.md \
                  $(MAN_SRC_DIR)/modbox-date.1.md \
                  $(MAN_SRC_DIR)/modbox-uniq.1.md \
                  $(MAN_SRC_DIR)/modbox-tee.1.md \
                  $(MAN_SRC_DIR)/modbox-wc.1.md \
                  $(MAN_SRC_DIR)/modbox-mkdir.1.md \
                  $(MAN_SRC_DIR)/modbox-chmod.1.md \
                  $(MAN_SRC_DIR)/modbox-chown.1.md \
                  $(MAN_SRC_DIR)/modbox-ln.1.md \
                  $(MAN_SRC_DIR)/modbox-dd.1.md \
                  $(MAN_SRC_DIR)/modbox-tr.1.md \
                  $(MAN_SRC_DIR)/modbox-uname.1.md \
                  $(MAN_SRC_DIR)/modbox-echo.1.md \
                  $(MAN_SRC_DIR)/modbox-xargs.1.md
MAN_PAGES := $(patsubst $(MAN_SRC_DIR)/%.md,$(MAN_BUILD_DIR)/%,$(MAN_SOURCES))
MAN_INSTALLED := $(patsubst $(MAN_SRC_DIR)/%.md,$(MAN_INSTALL_DIR)/%.gz,$(MAN_SOURCES))

# --------------------------------------------------------------------------
# Default target
# --------------------------------------------------------------------------
.PHONY: all
all: compile

# --------------------------------------------------------------------------
# Build
# --------------------------------------------------------------------------
.PHONY: compile
compile: $(TARGET)

# Link the executable from all object files.
$(TARGET): $(OBJ) | $(TARGET_DIR)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

# Compile one translation unit.
# compile_commands.json is an order-only prerequisite so clang-tidy (TIDY=1)
# can read it; it is never the cause of a rebuild.
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | compile_commands.json
	@mkdir -p $(dir $@)
ifeq ($(TIDY),1)
	-$(CLANG_TIDY) -p $(CURDIR) --use-color --system-headers=false $<
endif
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(TARGET_DIR):
	@mkdir -p $@

# --------------------------------------------------------------------------
# Header dependency files — drive smart rebuilds on header changes.
# --------------------------------------------------------------------------
-include $(DEP)

# --------------------------------------------------------------------------
# compile_commands.json (for clang-tidy / editors)
# Regenerated only when the source set or this Makefile changes.
# --------------------------------------------------------------------------
compile_commands.json: $(SRC) Makefile
	@printf '[' > $@
	@n=0; for f in $(SRC); do \
	  obj="$(BUILD_DIR)/$${f#$(SRC_DIR)/}"; obj="$${obj%.cpp}.o"; \
	  if [ $$n -gt 0 ]; then printf ',\n' >> $@; fi; \
	  printf '  {\n    "directory": "%s",\n    "command": "%s",\n    "file": "%s"\n  }' \
	    "$(CURDIR)" \
	    "$(CXX) -std=$(CXXSTD) $(CPPFLAGS) $(CXXFLAGS) -c $$f -o $$obj" \
	    "$$f" >> $@; \
	  n=$$((n+1)); \
	done
	@printf '\n]\n' >> $@

# --------------------------------------------------------------------------
# Lint
# --------------------------------------------------------------------------
.PHONY: lint
lint: compile_commands.json
ifeq ($(CLANG_TIDY),)
	@echo "clang-tidy not found; install it to enable linting"
else
	$(CLANG_TIDY) -p $(CURDIR) --use-color --system-headers=false $(SRC)
endif

# --------------------------------------------------------------------------
# Run
# --------------------------------------------------------------------------
.PHONY: run
run: compile
	./target/modbox

# --------------------------------------------------------------------------
# Man pages
# --------------------------------------------------------------------------
.PHONY: man
man: $(MAN_PAGES)

$(MAN_BUILD_DIR)/%: $(MAN_SRC_DIR)/%.md | $(MAN_BUILD_DIR)
ifeq ($(PANDOC),)
	@echo "pandoc not found; install it to generate man pages"
	@exit 1
endif
	$(PANDOC) -s -t man $< -o $@

$(MAN_BUILD_DIR):
	@mkdir -p $@

.PHONY: install-man
install-man: $(MAN_INSTALLED)

$(MAN_INSTALL_DIR)/%.gz: $(MAN_BUILD_DIR)/% | $(MAN_INSTALL_DIR)
	gzip -9 -c $< > $@

$(MAN_INSTALL_DIR):
	@mkdir -p $@

.PHONY: uninstall-man
uninstall-man:
	rm -f $(MAN_INSTALLED)

# --------------------------------------------------------------------------
# Clean / refresh
# --------------------------------------------------------------------------
.PHONY: clean
clean:
	rm -rf $(BUILD_DIR) $(TARGET_DIR) compile_commands.json
	rm -rf $(MAN_BUILD_DIR)

.PHONY: refresh
refresh: clean
	$(MAKE) compile

.PHONY: refresh-tidy
refresh-tidy: clean
	$(MAKE) compile TIDY=1
