# If RACK_DIR is not defined when calling the Makefile, default to two directories above
RACK_DIR ?= ../..

# FLAGS will be passed to both the C and C++ compiler
FLAGS +=
# FLAGS += -include force_link_glibc_2.23.h
CFLAGS +=
CXXFLAGS +=

# Careful about linking to shared libraries, since you can't assume much about the user's environment and library search path.
# Static libraries are fine, but they should be added to this plugin's build system.
LDFLAGS +=

# Add .cpp files to the build
#SOURCES += src/IMWidgets.cpp src/ImpromptuModular.cpp src/BigButtonSeq2.cpp src/FoundrySequencerKernel.cpp src/FoundrySequencer.cpp src/Foundry.cpp src/FoundryExpander.cpp
SOURCES += $(wildcard src/comp/*.cpp) $(wildcard src/*.cpp) 

# Add files to the ZIP package when running `make dist`
# The compiled plugin and "plugin.json" are automatically added.
DISTRIBUTABLES += res/comp res/fonts res/panels
DISTRIBUTABLES += $(wildcard LICENSE*)

# Include the Rack plugin Makefile framework
include $(RACK_DIR)/plugin.mk