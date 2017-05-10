include prorab.mk

this_name := checksvg

this_cxxflags += -g
this_cxxflags += -Wall
this_cxxflags += -Wno-format #no warnings about format
this_cxxflags += -DDEBUG
this_cxxflags += -fstrict-aliasing #strict aliasing!!!
this_cxxflags += -std=c++11

this_srcs += checksvg.cpp

this_ldlibs += -lsvgdom -lsvgren -lpapki `pkg-config --libs cairo` -lpng -lstdc++
this_ldflags += -L$(d)../../src

ifeq ($(prorab_os), macosx)
    this_cxxflags += -stdlib=libc++ #this is needed to be able to use c++11 std lib
    this_ldlibs += -lc++
else ifeq ($(prorab_os),windows)
    this_ldlibs += -lpugixml
endif

$(eval $(prorab-build-app))

this_svgs := $(patsubst %.svg,%,$(shell ls $(d)testdata/*.svg))

define this_rules
test:: $(prorab_this_name)
	@prorab-running-test.sh samples
	$(prorab_echo)cp $(d)../../src/*.dll . || true
	$(prorab_echo)for f in $(this_svgs); do \
	    echo "checking file $$$$f.svg..." && \
	    LD_LIBRARY_PATH=$(d)../../src $(d)./checksvg $$$$f.svg $$$$f.png && \
	    if [ $$$$? -ne 0 ]; then prorab-error.sh "rendered SVG did not match etalon sample"; exit 1; fi \
	done
	@prorab-passed.sh
endef
$(eval $(this_rules))


#add dependency on libsvgren
$(prorab_this_name): $(abspath $(d)../../src/libsvgren$(prorab_lib_extension))

$(eval $(call prorab-include,$(d)../../src/makefile))
