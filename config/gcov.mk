include $(config_dir)base/base.mk
include $(config_dir)base/agg.mk

# no optimization to avoid mismamtch of actual code to source lines,
# otherwise coverage report will not be accurate
this_cxxflags += -O0

this_cxxflags += -ftest-coverage
this_cxxflags += -fprofile-arcs

this_ldflags += --coverage
