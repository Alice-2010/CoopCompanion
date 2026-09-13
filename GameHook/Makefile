# Recursively find C++ sources without relying on platform-specific shell commands.
rwildcard = $(foreach path,$(wildcard $(1)*),$(call rwildcard,$(path)/,$(2)) $(filter $(subst *,%,$(2)),$(path)))
build_files := $(call rwildcard,./,*.cpp)
link_libs = d3d9.lib ws2_32.lib

.PHONY: build
build:
	@rm -rf bin
	@mkdir bin
	@cl /O2 /Fd:bin\\CoopCompanion.pdb /Fe:bin\\CoopCompanion.dll /Fo:bin\\ /std:c++20 $(build_files) /LD /link /MAP:bin\\CoopCompanion.map /DEBUG $(link_libs)
