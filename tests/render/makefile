include prorab.mk
include prorab-test.mk

this_name := tests

$(eval $(call prorab-config, ../../config))

this_srcs += main.cpp

this_ldlibs += -lsvgdom ../../src/out/$(c)/libsvgren$(dot_so) -lpapki -lpng -lutki -lm -lrasterimage
this_ldflags += -L../../src

ifeq ($(os), macosx)
else ifeq ($(os),windows)
else
    this_ldlibs += -lX11
endif

this_no_install := true

$(eval $(prorab-build-app))

ifeq ($(os),windows)
    this_run_cmd := $(a)cp $(d)../../src/out/$(c)/libsvgren$(dot_so) $(d) && $$^)
else
    this_run_cmd := $(a)(cd $(d); LD_LIBRARY_PATH=../../src/out/$(c) $$^)
endif

define this_rules
    run:: $(prorab_this_name)
$(.RECIPEPREFIX)@echo running $$^...
$(.RECIPEPREFIX)$(a)cp $(d)../../src/out/$(c)/libsvgren$(dot_so) . || true
$(.RECIPEPREFIX)$(a)LD_LIBRARY_PATH=$(d)../../src/out/$(c) $(d)out/$(c)/tests $(d)../samples/testdata/tiger.svg $(d)out.png
endef
$(eval $(this_rules))

# add dependency on libsvgren
$(prorab_this_name): $(abspath $(d)../../src/out/$(c)/libsvgren$(dot_so))

$(eval $(call prorab-include, ../../src/makefile))
