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
include examples/CMakeFiles/client_async.dir/depend.make

# Include the progress variables for this target.
include examples/CMakeFiles/client_async.dir/progress.make

# Include the compile flags for this target's objects.
include examples/CMakeFiles/client_async.dir/flags.make

examples/CMakeFiles/client_async.dir/client_async.c.o: examples/CMakeFiles/client_async.dir/flags.make
examples/CMakeFiles/client_async.dir/client_async.c.o: examples/client_async.c
	$(CMAKE_COMMAND) -E cmake_progress_report /home/sun/Downloads/open62541_sherylll/CMakeFiles $(CMAKE_PROGRESS_1)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building C object examples/CMakeFiles/client_async.dir/client_async.c.o"
	cd /home/sun/Downloads/open62541_sherylll/examples && /usr/bin/cc  $(C_DEFINES) $(C_FLAGS) -o CMakeFiles/client_async.dir/client_async.c.o   -c /home/sun/Downloads/open62541_sherylll/examples/client_async.c

examples/CMakeFiles/client_async.dir/client_async.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/client_async.dir/client_async.c.i"
	cd /home/sun/Downloads/open62541_sherylll/examples && /usr/bin/cc  $(C_DEFINES) $(C_FLAGS) -E /home/sun/Downloads/open62541_sherylll/examples/client_async.c > CMakeFiles/client_async.dir/client_async.c.i

examples/CMakeFiles/client_async.dir/client_async.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/client_async.dir/client_async.c.s"
	cd /home/sun/Downloads/open62541_sherylll/examples && /usr/bin/cc  $(C_DEFINES) $(C_FLAGS) -S /home/sun/Downloads/open62541_sherylll/examples/client_async.c -o CMakeFiles/client_async.dir/client_async.c.s

examples/CMakeFiles/client_async.dir/client_async.c.o.requires:
.PHONY : examples/CMakeFiles/client_async.dir/client_async.c.o.requires

examples/CMakeFiles/client_async.dir/client_async.c.o.provides: examples/CMakeFiles/client_async.dir/client_async.c.o.requires
	$(MAKE) -f examples/CMakeFiles/client_async.dir/build.make examples/CMakeFiles/client_async.dir/client_async.c.o.provides.build
.PHONY : examples/CMakeFiles/client_async.dir/client_async.c.o.provides

examples/CMakeFiles/client_async.dir/client_async.c.o.provides.build: examples/CMakeFiles/client_async.dir/client_async.c.o

# Object files for target client_async
client_async_OBJECTS = \
"CMakeFiles/client_async.dir/client_async.c.o"

# External object files for target client_async
client_async_EXTERNAL_OBJECTS =

bin/examples/client_async: examples/CMakeFiles/client_async.dir/client_async.c.o
bin/examples/client_async: examples/CMakeFiles/client_async.dir/build.make
bin/examples/client_async: bin/libopen62541.a
bin/examples/client_async: examples/CMakeFiles/client_async.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --red --bold "Linking C executable ../bin/examples/client_async"
	cd /home/sun/Downloads/open62541_sherylll/examples && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/client_async.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
examples/CMakeFiles/client_async.dir/build: bin/examples/client_async
.PHONY : examples/CMakeFiles/client_async.dir/build

examples/CMakeFiles/client_async.dir/requires: examples/CMakeFiles/client_async.dir/client_async.c.o.requires
.PHONY : examples/CMakeFiles/client_async.dir/requires

examples/CMakeFiles/client_async.dir/clean:
	cd /home/sun/Downloads/open62541_sherylll/examples && $(CMAKE_COMMAND) -P CMakeFiles/client_async.dir/cmake_clean.cmake
.PHONY : examples/CMakeFiles/client_async.dir/clean

examples/CMakeFiles/client_async.dir/depend:
	cd /home/sun/Downloads/open62541_sherylll && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/sun/Downloads/open62541_sherylll /home/sun/Downloads/open62541_sherylll/examples /home/sun/Downloads/open62541_sherylll /home/sun/Downloads/open62541_sherylll/examples /home/sun/Downloads/open62541_sherylll/examples/CMakeFiles/client_async.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : examples/CMakeFiles/client_async.dir/depend

