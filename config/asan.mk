include $(config_dir)rel.mk

this_cxxflags += -fsanitize=address
this_ldflags += -fsanitize=address
