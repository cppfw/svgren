include prorab.mk

this_name := tests

this_cxxflags += -g
this_cxxflags += -Wall
this_cxxflags += -Wno-format #no warnings about format
this_cxxflags += -DDEBUG
this_cxxflags += -fstrict-aliasing #strict aliasing!!!
this_cxxflags += -std=c++11

this_srcs += main.cpp

this_ldlibs += -lsvgdom $(d)../../src/libsvgren$(soext) -lpapki -lpng `pkg-config --libs cairo` -lstdc++
this_ldflags += -L$(d)../../src

ifeq ($(os), macosx)
    this_cxxflags += -stdlib=libc++ #this is needed to be able to use c++11 std lib
    this_ldlibs += -lc++
else ifeq ($(os),windows)
    this_ldlibs += -lpugixml
else
    this_ldlibs += -lX11
endif

$(eval $(prorab-build-app))


ifeq ($(os),windows)
    this_run_cmd := $(prorab_echo)cp $(d)../../src/libsvgren$(soext) $(d) && $$^)
else
    this_run_cmd := $(prorab_echo)(cd $(d); LD_LIBRARY_PATH=../../src $$^)
endif


define this_rules
run:: $(prorab_this_name)
	@echo running $$^...
	$(prorab_echo)cp $(d)../../src/libsvgren$(soext) . || true
	$(prorab_echo)LD_LIBRARY_PATH=$(d)../../src $(d)./tests $(svg)
endef
$(eval $(this_rules))

#add dependency on libsvgren
$(prorab_this_name): $(abspath $(d)../../src/libsvgren$(soext))

$(eval $(call prorab-include,$(d)../../src/makefile))
