include prorab.mk

$(eval $(call prorab-include, ../../src/makefile))

$(eval $(prorab-clear-this-vars))

this_name := checksvg

this_srcs += $(call prorab-src-dir, .)

$(eval $(call prorab-config, ../../config))

this_ldlibs += -lsvgdom $(d)../../src/out/$(c)/libsvgren$(dot_so) -lpapki -lclargs -lpng -lstdc++

ifeq ($(os), macosx)
    this_cxxflags += -stdlib=libc++ # this is needed to be able to use c++11 std lib
    this_ldlibs += -lc++
else ifeq ($(os),windows)
endif

this_no_install := true

$(eval $(prorab-build-app))

this_svgs := $(patsubst $(d)testdata/%.svg,%,$(shell ls $(d)testdata/*.svg))

this_dirs := $(subst /, ,$(d))
this_test := $(word $(words $(this_dirs)),$(this_dirs))

this_arch_bit_depth := $(shell getconf LONG_BIT)

# on 32 bit systems the actual rendered result differs a bit from PNG images generated on 64 bit system,
# so do this test only for 64 bit systems
ifeq ($(this_arch_bit_depth),64)
    define this_rules
        test:: $(prorab_this_name)
$(.RECIPEPREFIX)@myci-running-test.sh $(this_test)
$(.RECIPEPREFIX)$(a)cp $(d)../../src/out/$(c)/*.dll . || true
$(.RECIPEPREFIX)$(a)for f in $(this_svgs); do \
            echo "checking file $$$$f.svg" && \
            LD_LIBRARY_PATH=$(d)../../src/out/$(c) $(d)out/$(c)/checksvg --tolerance=10 $(d)testdata/$$$$f.svg $(d)testdata/$(this_svgren_backend)/$$$$f.png; \
            if [ $$$$? -ne 0 ]; then myci-error.sh "rendered SVG did not match etalon sample"; exit 1; fi \
        done
$(.RECIPEPREFIX)@myci-passed.sh
    endef
    $(eval $(this_rules))
endif

# add dependency on libsvgren
$(prorab_this_name): $(abspath $(d)../../src/out/$(c)/libsvgren$(dot_so))

# NOTE: the svgren makefile is uncluded in the beginning of this makefile