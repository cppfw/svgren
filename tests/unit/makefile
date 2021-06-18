include prorab.mk
include prorab-test.mk
include myci.mk

$(eval $(call myci-try-include, $(CONANBUILDINFO_DIR)conanbuildinfo.mak))

this_name := tests

this_srcs += $(call prorab-src-dir, .)

$(eval $(call prorab-config, ../../config))

this_ldlibs += -lsvgdom ../../src/out/$(c)/libsvgren$(dot_so) -lpng -lpapki -lutki -ltst -lm $(addprefix -l,$(CONAN_LIBS))
this_ldflags += -L../../src

this_cxxflags += $(addprefix -I,$(CONAN_INCLUDE_DIRS))
this_ldflags += $(addprefix -L,$(CONAN_LIB_DIRS))

this_no_install := true

$(eval $(prorab-build-app))

$(eval $(call prorab-depend, $(prorab_this_name), ../../src/out/$(c)/libsvgren$(dot_so)))

this_test_cmd := $(prorab_this_name) --junit-out=junit.xml
this_test_deps := $(prorab_this_name)
this_test_ld_path := ../../src/out/$(c) $(CONAN_LIB_DIRS)
$(eval $(prorab-test))

$(eval $(call prorab-include, ../../src/makefile))
