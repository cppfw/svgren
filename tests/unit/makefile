include prorab.mk
include prorab-test.mk

this_name := tests

this_srcs += $(call prorab-src-dir, .)

$(eval $(call prorab-config, ../../config))

this_ldlibs += -lsvgdom ../../src/out/$(c)/libsvgren$(dot_so) -lpng -lpapki -lutki -ltst -lm -lrasterimage
this_ldflags += -L../../src

this_no_install := true

$(eval $(prorab-build-app))

$(eval $(call prorab-depend, $(prorab_this_name), ../../src/out/$(c)/libsvgren$(dot_so)))

this_test_cmd := $(prorab_this_name) --junit-out=out/$(c)/junit.xml
this_test_deps := $(prorab_this_name)
this_test_ld_path := ../../src/out/$(c)
$(eval $(prorab-test))

$(eval $(call prorab-include, ../../src/makefile))
