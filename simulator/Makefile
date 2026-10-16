# Web simulator build: compiles LVGL + the firmware's portable code + the
# simulator board layer to WebAssembly. Run through build.sh (it prepares the
# emsdk environment and downloads LVGL first).
HERE  := $(CURDIR)
FW    := $(abspath $(HERE)/../firmware)
LVGL  := $(HERE)/.deps/lvgl-8.4.0
OUT   := $(HERE)/.build
WEB   := $(HERE)/web

LVGL_SRCS := $(shell find $(LVGL)/src -name '*.c')
FW_C      := $(wildcard $(FW)/src/ui/font_aurebesh_*.c)
FW_CPP    := $(FW)/src/main.cpp $(FW)/src/i18n.cpp \
             $(wildcard $(FW)/src/ui/*.cpp) $(wildcard $(FW)/src/ui/screens/*.cpp)
SIM_CPP   := $(wildcard $(HERE)/src/*.cpp)

DEFS  := -DBOARD_SIM -DLV_CONF_INCLUDE_SIMPLE
INCS  := -I$(HERE)/shim -I$(FW)/include -I$(FW)/src -I$(LVGL) -I$(LVGL)/src
CFLAGS   := -O2 $(DEFS) $(INCS) -w
CXXFLAGS := -O2 -std=gnu++17 $(DEFS) $(INCS) -Wno-unused-result

# One object per source, flattened into .build/ with a path-derived name
objname = $(OUT)/$(subst /,_,$(patsubst $(abspath $(HERE)/..)/%,%,$(1))).o
C_SRCS   := $(LVGL_SRCS) $(FW_C)
CPP_SRCS := $(FW_CPP) $(SIM_CPP)
OBJS := $(foreach s,$(C_SRCS) $(CPP_SRCS),$(call objname,$(s)))

EXPORTS := _sim_start,_sim_tick,_sim_configure,_sim_pointer,_sim_fb,_sim_w,_sim_h,_sim_frames,_sim_loops

all: $(WEB)/datapad.js

$(WEB)/datapad.js: $(OBJS)
	@mkdir -p $(WEB)
	em++ -O2 $(OBJS) -o $@ --no-entry \
	  -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=64MB -sENVIRONMENT=web \
	  -sEXPORTED_FUNCTIONS=$(EXPORTS) -sEXPORTED_RUNTIME_METHODS=ccall,cwrap,UTF8ToString,HEAPU8 \
	  -sSTACK_SIZE=1MB

define c_rule
$(call objname,$(1)): $(1)
	@mkdir -p $(OUT)
	emcc $(CFLAGS) -c $(1) -o $$@
endef
define cpp_rule
$(call objname,$(1)): $(1)
	@mkdir -p $(OUT)
	em++ $(CXXFLAGS) -c $(1) -o $$@
endef
$(foreach s,$(C_SRCS),$(eval $(call c_rule,$(s))))
$(foreach s,$(CPP_SRCS),$(eval $(call cpp_rule,$(s))))

clean:
	rm -rf $(OUT) $(WEB)/datapad.js $(WEB)/datapad.wasm
.PHONY: all clean
