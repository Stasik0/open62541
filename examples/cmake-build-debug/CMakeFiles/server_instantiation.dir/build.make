# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.17

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:


# Disable VCS-based implicit rules.
% : %,v


# Disable VCS-based implicit rules.
% : RCS/%


# Disable VCS-based implicit rules.
% : RCS/%,v


# Disable VCS-based implicit rules.
% : SCCS/s.%


# Disable VCS-based implicit rules.
% : s.%


.SUFFIXES: .hpux_make_needs_suffix_list


# Command-line flag to silence nested $(MAKE).
$(VERBOSE)MAKESILENT = -s

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
CMAKE_COMMAND = /snap/clion/138/bin/cmake/linux/bin/cmake

# The command to remove a file.
RM = /snap/clion/138/bin/cmake/linux/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/ammar/open62541/examples

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/ammar/open62541/examples/cmake-build-debug

# Include any dependencies generated for this target.
include CMakeFiles/server_instantiation.dir/depend.make

# Include the progress variables for this target.
include CMakeFiles/server_instantiation.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/server_instantiation.dir/flags.make

CMakeFiles/server_instantiation.dir/server_instantiation.c.o: CMakeFiles/server_instantiation.dir/flags.make
CMakeFiles/server_instantiation.dir/server_instantiation.c.o: ../server_instantiation.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/ammar/open62541/examples/cmake-build-debug/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building C object CMakeFiles/server_instantiation.dir/server_instantiation.c.o"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/server_instantiation.dir/server_instantiation.c.o   -c /home/ammar/open62541/examples/server_instantiation.c

CMakeFiles/server_instantiation.dir/server_instantiation.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/server_instantiation.dir/server_instantiation.c.i"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/ammar/open62541/examples/server_instantiation.c > CMakeFiles/server_instantiation.dir/server_instantiation.c.i

CMakeFiles/server_instantiation.dir/server_instantiation.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/server_instantiation.dir/server_instantiation.c.s"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/ammar/open62541/examples/server_instantiation.c -o CMakeFiles/server_instantiation.dir/server_instantiation.c.s

# Object files for target server_instantiation
server_instantiation_OBJECTS = \
"CMakeFiles/server_instantiation.dir/server_instantiation.c.o"

# External object files for target server_instantiation
server_instantiation_EXTERNAL_OBJECTS =

bin/examples/server_instantiation: CMakeFiles/server_instantiation.dir/server_instantiation.c.o
bin/examples/server_instantiation: CMakeFiles/server_instantiation.dir/build.make
bin/examples/server_instantiation: /usr/lib/x86_64-linux-gnu/libopen62541.so.1.1.5
bin/examples/server_instantiation: CMakeFiles/server_instantiation.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/ammar/open62541/examples/cmake-build-debug/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking C executable bin/examples/server_instantiation"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/server_instantiation.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/server_instantiation.dir/build: bin/examples/server_instantiation

.PHONY : CMakeFiles/server_instantiation.dir/build

CMakeFiles/server_instantiation.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/server_instantiation.dir/cmake_clean.cmake
.PHONY : CMakeFiles/server_instantiation.dir/clean

CMakeFiles/server_instantiation.dir/depend:
	cd /home/ammar/open62541/examples/cmake-build-debug && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/ammar/open62541/examples /home/ammar/open62541/examples /home/ammar/open62541/examples/cmake-build-debug /home/ammar/open62541/examples/cmake-build-debug /home/ammar/open62541/examples/cmake-build-debug/CMakeFiles/server_instantiation.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/server_instantiation.dir/depend

