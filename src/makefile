include prorab.mk
include myci.mk

$(eval $(call myci-try-include, $(CONANBUILDINFO_DIR)conanbuildinfo.mak))

this_name := svgren

this_soname := $(shell cat $(d)soname.txt)

this_srcs := $(call prorab-src-dir,.)

# this_cxxflags += -D SVGREN_BACKGROUND=0xffffffff
# this_cxxflags += -D SVGREN_BACKGROUND=0xff00ffff

$(eval $(call prorab-config, ../config))

ifeq ($(this_svgren_backend),agg)
    this_cxxflags += -D SVGREN_BACKEND=SVGREN_BACKEND_AGG
    this_ldlibs += `pkg-config --libs agg`
else ifeq ($(this_svgren_backend),cairo)
    this_cxxflags += -D SVGREN_BACKEND=SVGREN_BACKEND_CAIRO
    ifeq ($(os),windows)
        this_cxxflags += `pkg-config --cflags cairo`
    endif
    this_ldlibs += `pkg-config --libs cairo`
endif

this_ldlibs += -lsvgdom -lstdc++ -lm

this_cxxflags += $(addprefix -I,$(CONAN_INCLUDE_DIRS))
this_cxxflags_test += $(addprefix -I,$(CONAN_INCLUDE_DIRS))
this_ldflags += $(addprefix -L,$(CONAN_LIB_DIRS))

$(eval $(prorab-build-lib))
