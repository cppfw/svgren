include prorab.mk


debug := false


this_name := svgren

this_soname := $(shell cat $(prorab_this_dir)soname.txt)


#Sources
this_srcs += svgren/render.cpp




this_cxxflags := 

ifeq ($(debug),true)
    this_cxxflags += -DDEBUG
endif




this_cxxflags += -Wall #enable all warnings
this_cxxflags += -Wno-comment #no warnings on nested comments
this_cxxflags += -funsigned-char #the 'char' type is unsigned
this_cxxflags += -Wnon-virtual-dtor #warn if base class has non-virtual destructor
this_cxxflags += -fno-operator-names #do not treat 'and', 'bitand','bitor','compl','not','or','xor' as keywords
this_cxxflags += -Werror #treat warnings as errors
this_cxxflags += -Wfatal-errors # stop on first error encountered
this_cxxflags += -fstrict-aliasing #strict aliasing (in order to comply with the c++ standard more strictly)
this_cxxflags += -std=c++11
this_cxxflags += -g #include debugging symbols
                #-P -E #-Wreorder
                #-O3 -funroll-loops -fomit-frame-pointer

this_ldlibs += -lsvgdom -lcairo -lstdc++ -lm

ifeq ($(prorab_os),linux)
    this_cxxflags += -fPIC # Since we are building shared library, we need Position-Independend Code
else ifeq ($(prorab_os),windows)
    this_cxxflags += -I/usr/include -I/usr/local/include
    this_ldflags += -L/usr/lib -L/usr/local/lib
else ifeq ($(prorab_os),macosx)
    this_cxxflags += -stdlib=libc++ #this is needed to be able to use c++11 std lib
endif



$(eval $(prorab-build-lib))
