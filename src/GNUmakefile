# The name of the program
PROJNAME = Snowscape

# Build configuration
export ELFOBJECTTYPE ?= HARDFPU
export OPTIMIZE ?= yes

# Set compiler flags based on ELFOBJECTTYPE (matching OSLib build system)
ifeq ($(OPTIMIZE),yes)
OPTFLAGS = -O2
else
OPTFLAGS = -g
endif

ifeq ($(ELFOBJECTTYPE),SOFTFPU)
CFLAGS = $(OPTFLAGS) -mfloat-abi=soft
else ifeq ($(ELFOBJECTTYPE),HARDFPU)
CFLAGS = $(OPTFLAGS) -mlibscl
else ifeq ($(ELFOBJECTTYPE),MODULE)
CFLAGS = $(OPTFLAGS) -mmodule
else
$(error Unknown ELFOBJECTTYPE value, possible values are SOFTFPU, HARDFPU and MODULE)
endif

# Feature flags - default to no if not specified
USE_256_COLORS ?= no
TARGET_A5000 ?= no

ifeq ($(TARGET_A5000),yes)
CFLAGS += -DA5000
endif

ifeq ($(USE_256_COLORS),yes)
CFLAGS += -DPAL_256
ASFLAGS = -Wa,--defsym,PAL_256=1
else
ASFLAGS = 
endif

#------------------------------------------------------------------------
# Paths & directories
SRCDIR := ./
OBJDIR := ./obj
DEPDIR := $(OBJDIR)/.deps

# Include paths
INCLUDES := -I$(GCCSDK_INSTALL_ENV)/include

# Libraries and linking
LIBS := -lm
LDFLAGS := -L$(GCCSDK_INSTALL_ENV)/lib -static

# Source files (.c or .s)
SRCFILES = main.c math3d.c mesh.c palette.c render.c poly.s

# Generate object and dependency file names
OBJFILES = $(patsubst %.c,$(OBJDIR)/%.o,$(patsubst %.s,$(OBJDIR)/%.o,$(SRCFILES)))
DEPFILES = $(patsubst %.c,$(DEPDIR)/%.d,$(filter %.c,$(SRCFILES)))

# Header files for dependency tracking
HEADERS = $(wildcard *.h)

#------------------------------------------------------------------------
# Tools
GCC = $(wildcard $(GCCSDK_INSTALL_CROSSBIN)/*gcc)
RM = rm -f
MKDIR_P = mkdir -p

#------------------------------------------------------------------------
# Targets

# Default target
all: !$(PROJNAME)/!RunImage,ff8

# Debug build target
debug: OPTIMIZE = no
debug: clean all

# Release build target  
release: OPTIMIZE = yes
release: clean all

# Create directories if they don't exist
$(OBJDIR) $(DEPDIR):
	$(MKDIR_P) $@

# Link the ELF object files into an AIF executable
!$(PROJNAME)/!RunImage,ff8: $(OBJFILES)
	$(GCC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

# Pattern rule for C sources with dependency generation
$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR) $(DEPDIR)
	$(GCC) -c $(CFLAGS) $(INCLUDES) -MMD -MP -MF $(DEPDIR)/$*.d $< -o $@

# Pattern rule for assembly sources
$(OBJDIR)/%.o: $(SRCDIR)/%.s | $(OBJDIR)
	$(GCC) -c $(CFLAGS) $(ASFLAGS) $(INCLUDES) $< -o $@

# Include dependency files if they exist
-include $(DEPFILES)

# Clean target
clean:
	$(RM) $(OBJFILES) $(DEPFILES) !$(PROJNAME)/!RunImage,ff8
	$(RM) -d $(DEPDIR) $(OBJDIR) 2>/dev/null || true

# Force rebuild of all targets
rebuild: clean all

# Show build configuration
show-config:
	@echo "Project: $(PROJNAME)"
	@echo "ELFOBJECTTYPE: $(ELFOBJECTTYPE)"
	@echo "OPTIMIZE: $(OPTIMIZE)" 
	@echo "USE_256_COLORS: $(USE_256_COLORS)"
	@echo "TARGET_A5000: $(TARGET_A5000)"
	@echo "CFLAGS: $(CFLAGS)"
	@echo "ASFLAGS: $(ASFLAGS)"
	@echo "Compiler: $(GCC)"

.PHONY: all debug release clean rebuild show-config