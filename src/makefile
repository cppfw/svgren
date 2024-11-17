include prorab.mk
include prorab-license.mk
include prorab-clang-format.mk
include prorab-install-dbgsrc.mk

this_name := svgren

this_soname := $(shell cat $(d)soname.txt)

this_srcs := $(call prorab-src-dir,.)

# this_cxxflags += -D SVGREN_BACKGROUND=0xffffffff
# this_cxxflags += -D SVGREN_BACKGROUND=0xff00ffff

$(eval $(call prorab-config, ../config))

this_ldlibs += -lsvgdom -lutki -lstdc++ -lm
this_ldlibs += -l veg

$(eval $(prorab-build-lib))

this_src_dir := $(this_name)
this_license_file := ../LICENSE
$(eval $(prorab-license))

$(eval $(prorab-clang-format))

$(eval $(prorab-install-dbgsrc))
