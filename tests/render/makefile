include prorab.mk
include prorab-test.mk

$(eval $(call prorab-config, ../../config))

this_name := tests

this_srcs += main.cpp

this_ldlibs += -l svgdom$(this_dbg)
this_ldlibs += -l rasterimage$(this_dbg)
this_ldlibs += -l papki$(this_dbg)
this_ldlibs += -l utki$(this_dbg)

this_ldlibs += -l png
this_ldlibs += -l m

this_ldlibs += ../../src/out/$(c)/libsvgren$(this_dbg)$(dot_so)

ifeq ($(os), macosx)
else ifeq ($(os),windows)
else
    this_ldlibs += -l X11
endif

this_no_install := true

$(eval $(prorab-build-app))

# TODO: use prorab-run
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

$(eval $(call prorab-include, ../../src/makefile))
