include prorab.mk
include prorab-test.mk

this_name := tests

this_srcs += main.cpp

$(eval $(call prorab-config, ../../config))

this_ldlibs += -lsvgdom ../../src/out/$(c)/libsvgren$(dot_so) -lpapki -lpng -lutki -lpthread

this_no_install := true

$(eval $(prorab-build-app))

# add dependency on libsvgren
$(prorab_this_name): $(abspath $(d)../../src/out/$(c)/libsvgren$(dot_so))

this_test_cmd := $(prorab_this_name) $(shell ls $(d)../unit/samples_data/*.svg)
this_test_deps := $(prorab_this_name)
this_test_ld_path := ../../src/out/$(c)
$(eval $(prorab-test))

$(eval $(call prorab-include, ../../src/makefile))
