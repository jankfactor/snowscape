###############################################################################
# Setting up ArchieSDK
ifeq ($(strip $(ARCHIESDK)),)
$(error "ARCHIESDK was not found in your environment. please export ARCHIESDK")
endif
include $(ARCHIESDK)/config.mk
###############################################################################

# Project configuration
APPNAME = !RunImage
PROJNAME = Snowscape

# Directories
SRCDIR := ./
OBJDIR := ./obj
DEPDIR := $(OBJDIR)/.deps
TARGETDIR := ./!$(PROJNAME)

# Source files (.c or .s)
SRCFILES = main.c math3d.c mesh.c palette.c render.c poly.s

# Libraries
LIBS = -lm

# Build optimization - default to release
OPTIMIZE ?= yes

# Feature flags - default to no if not specified
USE_256_COLORS ?= no
TARGET_A5000 ?= no

# Set optimization flags
ifeq ($(OPTIMIZE),yes)
CFLAGS += -O2
else
CFLAGS += -g
endif

# Apply feature flags
ifeq ($(TARGET_A5000),yes)
CFLAGS += -DA5000
endif

ifeq ($(USE_256_COLORS),yes)
CFLAGS += -DPAL_256
ASFLAGS += -Wa,--defsym,PAL_256=1
else
ASFLAGS += 
endif

# Object and dependency files
OBJFILES = $(patsubst %.c,$(OBJDIR)/%.o,$(patsubst %.s,$(OBJDIR)/%.o,$(SRCFILES)))
DEPFILES = $(patsubst %.c,$(DEPDIR)/%.d,$(filter %.c,$(SRCFILES)))

# Tools
RM = rm -f
MKDIR_P = mkdir -p

#------------------------------------------------------------------------
# Targets

# Default target
all: $(TARGETDIR)/$(APPNAME),ff8

# Debug build target
debug: OPTIMIZE = no
debug: clean all

# Release build target  
release: OPTIMIZE = yes
release: clean all

# Create directories if they don't exist
$(OBJDIR) $(DEPDIR) $(TARGETDIR):
	$(MKDIR_P) $@

# Build with separate compilation and linking for better dependency handling
$(TARGETDIR)/$(APPNAME),ff8: $(OBJFILES) | $(TARGETDIR)
	$(ARCHIECC) $(CFLAGS) -o $(TARGETDIR)/tmpProg $(OBJFILES) $(LIBS)
	$(ARCHIEOBJCOPY) -O binary $(TARGETDIR)/tmpProg $@
	$(RM) $(TARGETDIR)/tmpProg

# Pattern rule for C sources with dependency generation
$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR) $(DEPDIR)
	$(ARCHIECC) -c $(CFLAGS) -MMD -MP -MF $(DEPDIR)/$*.d $< -o $@

# Pattern rule for assembly sources
$(OBJDIR)/%.o: $(SRCDIR)/%.s | $(OBJDIR)
	$(ARCHIECC) -c $(CFLAGS) $(ASFLAGS) $< -o $@

# Include dependency files if they exist
-include $(DEPFILES)

# Generate ASM from C files with better organization
asm: | $(OBJDIR)
	$(ARCHIECC) $(CFLAGS) $(ASFLAGS) -S $(filter %.c,$(SRCFILES)) -o $(OBJDIR)/

# Clean target
clean:
	$(RM) $(OBJFILES) $(DEPFILES) $(TARGETDIR)/$(APPNAME),ff8 $(TARGETDIR)/tmpProg
	$(RM) -d $(DEPDIR) $(OBJDIR) 2>/dev/null || true

# Force rebuild of all targets
rebuild: clean all

# Show build configuration
show-config:
	@echo "Project: $(PROJNAME)"
	@echo "App Name: $(APPNAME)"
	@echo "ArchieSDK: $(ARCHIESDK)"
	@echo "OPTIMIZE: $(OPTIMIZE)" 
	@echo "USE_256_COLORS: $(USE_256_COLORS)"
	@echo "TARGET_A5000: $(TARGET_A5000)"
	@echo "CFLAGS: $(CFLAGS)"
	@echo "ASFLAGS: $(ASFLAGS)"
	@echo "Compiler: $(ARCHIECC)"
	@echo "Object Copy: $(ARCHIEOBJCOPY)"

# Install/copy target (matches build.sh behavior)
install: all
	@if [ -n "$(TARGETCOPY)" ]; then \
		echo "Copying to $(TARGETCOPY)/!$(PROJNAME)"; \
		$(MKDIR_P) "$(TARGETCOPY)/!$(PROJNAME)"; \
		cp -rf $(TARGETDIR)/* "$(TARGETCOPY)/!$(PROJNAME)/"; \
	else \
		echo "TARGETCOPY not set - skipping install"; \
	fi

.PHONY: all debug release clean rebuild asm show-config install