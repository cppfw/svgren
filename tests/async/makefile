include prorab.mk
include prorab-test.mk

$(eval $(call prorab-config, ../../config))

this_name := tests

this_srcs += main.cpp

this_ldlibs += -l svgdom$(this_dbg)
this_ldlibs += -l papki$(this_dbg)
this_ldlibs += -l utki$(this_dbg)
this_ldlibs += -l png
this_ldlibs += -l pthread

this_ldlibs += ../../src/out/$(c)/libsvgren$(this_dbg)$(dot_so)

this_no_install := true

$(eval $(prorab-build-app))

this_test_cmd := $(prorab_this_name) $(shell ls $(d)../unit/samples_data/*.svg)
this_test_deps := $(prorab_this_name)
this_test_ld_path := ../../src/out/$(c)
$(eval $(prorab-test))

$(eval $(call prorab-include, ../../src/makefile))
