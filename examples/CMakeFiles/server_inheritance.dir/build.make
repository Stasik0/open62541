# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.2

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:

# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list

# Suppress display of executed commands.
$(VERBOSE).SILENT:

# A target that is always out of date.
cmake_force:
.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/sun/Downloads/open62541_sherylll

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/sun/Downloads/open62541_sherylll

# Include any dependencies generated for this target.
include examples/CMakeFiles/server_inheritance.dir/depend.make

# Include the progress variables for this target.
include examples/CMakeFiles/server_inheritance.dir/progress.make

# Include the compile flags for this target's objects.
include examples/CMakeFiles/server_inheritance.dir/flags.make

examples/CMakeFiles/server_inheritance.dir/server_inheritance.c.o: examples/CMakeFiles/server_inheritance.dir/flags.make
examples/CMakeFiles/server_inheritance.dir/server_inheritance.c.o: examples/server_inheritance.c
	$(CMAKE_COMMAND) -E cmake_progress_report /home/sun/Downloads/open62541_sherylll/CMakeFiles $(CMAKE_PROGRESS_1)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building C object examples/CMakeFiles/server_inheritance.dir/server_inheritance.c.o"
	cd /home/sun/Downloads/open62541_sherylll/examples && /usr/bin/cc  $(C_DEFINES) $(C_FLAGS) -o CMakeFiles/server_inheritance.dir/server_inheritance.c.o   -c /home/sun/Downloads/open62541_sherylll/examples/server_inheritance.c

examples/CMakeFiles/server_inheritance.dir/server_inheritance.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/server_inheritance.dir/server_inheritance.c.i"
	cd /home/sun/Downloads/open62541_sherylll/examples && /usr/bin/cc  $(C_DEFINES) $(C_FLAGS) -E /home/sun/Downloads/open62541_sherylll/examples/server_inheritance.c > CMakeFiles/server_inheritance.dir/server_inheritance.c.i

examples/CMakeFiles/server_inheritance.dir/server_inheritance.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/server_inheritance.dir/server_inheritance.c.s"
	cd /home/sun/Downloads/open62541_sherylll/examples && /usr/bin/cc  $(C_DEFINES) $(C_FLAGS) -S /home/sun/Downloads/open62541_sherylll/examples/server_inheritance.c -o CMakeFiles/server_inheritance.dir/server_inheritance.c.s

examples/CMakeFiles/server_inheritance.dir/server_inheritance.c.o.requires:
.PHONY : examples/CMakeFiles/server_inheritance.dir/server_inheritance.c.o.requires

examples/CMakeFiles/server_inheritance.dir/server_inheritance.c.o.provides: examples/CMakeFiles/server_inheritance.dir/server_inheritance.c.o.requires
	$(MAKE) -f examples/CMakeFiles/server_inheritance.dir/build.make examples/CMakeFiles/server_inheritance.dir/server_inheritance.c.o.provides.build
.PHONY : examples/CMakeFiles/server_inheritance.dir/server_inheritance.c.o.provides

examples/CMakeFiles/server_inheritance.dir/server_inheritance.c.o.provides.build: examples/CMakeFiles/server_inheritance.dir/server_inheritance.c.o

# Object files for target server_inheritance
server_inheritance_OBJECTS = \
"CMakeFiles/server_inheritance.dir/server_inheritance.c.o"

# External object files for target server_inheritance
server_inheritance_EXTERNAL_OBJECTS =

bin/examples/server_inheritance: examples/CMakeFiles/server_inheritance.dir/server_inheritance.c.o
bin/examples/server_inheritance: examples/CMakeFiles/server_inheritance.dir/build.make
bin/examples/server_inheritance: bin/libopen62541.a
bin/examples/server_inheritance: examples/CMakeFiles/server_inheritance.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --red --bold "Linking C executable ../bin/examples/server_inheritance"
	cd /home/sun/Downloads/open62541_sherylll/examples && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/server_inheritance.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
examples/CMakeFiles/server_inheritance.dir/build: bin/examples/server_inheritance
.PHONY : examples/CMakeFiles/server_inheritance.dir/build

examples/CMakeFiles/server_inheritance.dir/requires: examples/CMakeFiles/server_inheritance.dir/server_inheritance.c.o.requires
.PHONY : examples/CMakeFiles/server_inheritance.dir/requires

examples/CMakeFiles/server_inheritance.dir/clean:
	cd /home/sun/Downloads/open62541_sherylll/examples && $(CMAKE_COMMAND) -P CMakeFiles/server_inheritance.dir/cmake_clean.cmake
.PHONY : examples/CMakeFiles/server_inheritance.dir/clean

examples/CMakeFiles/server_inheritance.dir/depend:
	cd /home/sun/Downloads/open62541_sherylll && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/sun/Downloads/open62541_sherylll /home/sun/Downloads/open62541_sherylll/examples /home/sun/Downloads/open62541_sherylll /home/sun/Downloads/open62541_sherylll/examples /home/sun/Downloads/open62541_sherylll/examples/CMakeFiles/server_inheritance.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : examples/CMakeFiles/server_inheritance.dir/depend

