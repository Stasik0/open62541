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
include examples/CMakeFiles/access_control_server.dir/depend.make

# Include the progress variables for this target.
include examples/CMakeFiles/access_control_server.dir/progress.make

# Include the compile flags for this target's objects.
include examples/CMakeFiles/access_control_server.dir/flags.make

examples/CMakeFiles/access_control_server.dir/access_control/server_access_control.c.o: examples/CMakeFiles/access_control_server.dir/flags.make
examples/CMakeFiles/access_control_server.dir/access_control/server_access_control.c.o: examples/access_control/server_access_control.c
	$(CMAKE_COMMAND) -E cmake_progress_report /home/sun/Downloads/open62541_sherylll/CMakeFiles $(CMAKE_PROGRESS_1)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building C object examples/CMakeFiles/access_control_server.dir/access_control/server_access_control.c.o"
	cd /home/sun/Downloads/open62541_sherylll/examples && /usr/bin/cc  $(C_DEFINES) $(C_FLAGS) -o CMakeFiles/access_control_server.dir/access_control/server_access_control.c.o   -c /home/sun/Downloads/open62541_sherylll/examples/access_control/server_access_control.c

examples/CMakeFiles/access_control_server.dir/access_control/server_access_control.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/access_control_server.dir/access_control/server_access_control.c.i"
	cd /home/sun/Downloads/open62541_sherylll/examples && /usr/bin/cc  $(C_DEFINES) $(C_FLAGS) -E /home/sun/Downloads/open62541_sherylll/examples/access_control/server_access_control.c > CMakeFiles/access_control_server.dir/access_control/server_access_control.c.i

examples/CMakeFiles/access_control_server.dir/access_control/server_access_control.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/access_control_server.dir/access_control/server_access_control.c.s"
	cd /home/sun/Downloads/open62541_sherylll/examples && /usr/bin/cc  $(C_DEFINES) $(C_FLAGS) -S /home/sun/Downloads/open62541_sherylll/examples/access_control/server_access_control.c -o CMakeFiles/access_control_server.dir/access_control/server_access_control.c.s

examples/CMakeFiles/access_control_server.dir/access_control/server_access_control.c.o.requires:
.PHONY : examples/CMakeFiles/access_control_server.dir/access_control/server_access_control.c.o.requires

examples/CMakeFiles/access_control_server.dir/access_control/server_access_control.c.o.provides: examples/CMakeFiles/access_control_server.dir/access_control/server_access_control.c.o.requires
	$(MAKE) -f examples/CMakeFiles/access_control_server.dir/build.make examples/CMakeFiles/access_control_server.dir/access_control/server_access_control.c.o.provides.build
.PHONY : examples/CMakeFiles/access_control_server.dir/access_control/server_access_control.c.o.provides

examples/CMakeFiles/access_control_server.dir/access_control/server_access_control.c.o.provides.build: examples/CMakeFiles/access_control_server.dir/access_control/server_access_control.c.o

# Object files for target access_control_server
access_control_server_OBJECTS = \
"CMakeFiles/access_control_server.dir/access_control/server_access_control.c.o"

# External object files for target access_control_server
access_control_server_EXTERNAL_OBJECTS =

bin/examples/access_control_server: examples/CMakeFiles/access_control_server.dir/access_control/server_access_control.c.o
bin/examples/access_control_server: examples/CMakeFiles/access_control_server.dir/build.make
bin/examples/access_control_server: bin/libopen62541.a
bin/examples/access_control_server: examples/CMakeFiles/access_control_server.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --red --bold "Linking C executable ../bin/examples/access_control_server"
	cd /home/sun/Downloads/open62541_sherylll/examples && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/access_control_server.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
examples/CMakeFiles/access_control_server.dir/build: bin/examples/access_control_server
.PHONY : examples/CMakeFiles/access_control_server.dir/build

examples/CMakeFiles/access_control_server.dir/requires: examples/CMakeFiles/access_control_server.dir/access_control/server_access_control.c.o.requires
.PHONY : examples/CMakeFiles/access_control_server.dir/requires

examples/CMakeFiles/access_control_server.dir/clean:
	cd /home/sun/Downloads/open62541_sherylll/examples && $(CMAKE_COMMAND) -P CMakeFiles/access_control_server.dir/cmake_clean.cmake
.PHONY : examples/CMakeFiles/access_control_server.dir/clean

examples/CMakeFiles/access_control_server.dir/depend:
	cd /home/sun/Downloads/open62541_sherylll && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/sun/Downloads/open62541_sherylll /home/sun/Downloads/open62541_sherylll/examples /home/sun/Downloads/open62541_sherylll /home/sun/Downloads/open62541_sherylll/examples /home/sun/Downloads/open62541_sherylll/examples/CMakeFiles/access_control_server.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : examples/CMakeFiles/access_control_server.dir/depend

