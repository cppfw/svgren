include $(config_dir)rel.mk

this_cxxflags += -fsanitize=address
this_ldflags += -fsanitize=address

this_lint_cmd = $(prorab_lint_cmd_clang_tidy)
