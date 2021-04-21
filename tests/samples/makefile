include prorab.mk
include prorab-test.mk
include myci.mk

$(eval $(call myci-try-include, $(CONANBUILDINFO_DIR)conanbuildinfo.mak))

$(eval $(call prorab-include, ../../src/makefile))

$(eval $(prorab-clear-this-vars))

this_name := checksvg

this_srcs += $(call prorab-src-dir, .)

$(eval $(call prorab-config, ../../config))

this_ldlibs += -lsvgdom $(d)../../src/out/$(c)/libsvgren$(dot_so) -lpapki -lclargs -lpng -lutki -lm

this_cxxflags += $(addprefix -I,$(CONAN_INCLUDE_DIRS))
this_ldflags += $(addprefix -L,$(CONAN_LIB_DIRS))

this_no_install := true

$(eval $(prorab-build-app))

# add dependency on libsvgren
$(prorab_this_name): $(abspath $(d)../../src/out/$(c)/libsvgren$(dot_so))

this_arch_bit_depth := $(shell getconf LONG_BIT)

# on 32 bit systems the actual rendered result differs a bit from PNG images generated on 64 bit system,
# so do this test only for 64 bit systems
ifeq ($(this_arch_bit_depth),64)
    this_test_cmd := for f in $(patsubst $(d)testdata/%.svg,%,$(shell ls $(d)testdata/*.svg)); do \
            echo "checking file $$$$f.svg" && \
            $(prorab_this_name) --tolerance=10 $(d)testdata/$$$$f.svg $(d)testdata/$(this_svgren_backend)/$$$$f.png; \
            if [ $$$$? -ne 0 ]; then myci-error.sh "rendered SVG did not match etalon sample"; exit 1; fi \
        done
    this_test_deps := $(prorab_this_name)
    this_test_ld_path := ../../src/out/$(c) $(CONAN_LIB_DIRS)
    $(eval $(prorab-test))
endif

# NOTE: the svgren makefile is included in the beginning of this makefile