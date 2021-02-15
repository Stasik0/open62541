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
include nodeset/CMakeFiles/server_nodeset.dir/depend.make

# Include the progress variables for this target.
include nodeset/CMakeFiles/server_nodeset.dir/progress.make

# Include the compile flags for this target's objects.
include nodeset/CMakeFiles/server_nodeset.dir/flags.make

src_generated/open62541/namespace_example_generated.c: /usr/share/open62541/tools/nodeset_compiler/nodeset_compiler.py
src_generated/open62541/namespace_example_generated.c: /usr/share/open62541/tools/nodeset_compiler/nodes.py
src_generated/open62541/namespace_example_generated.c: /usr/share/open62541/tools/nodeset_compiler/nodeset.py
src_generated/open62541/namespace_example_generated.c: /usr/share/open62541/tools/nodeset_compiler/datatypes.py
src_generated/open62541/namespace_example_generated.c: /usr/share/open62541/tools/nodeset_compiler/backend_open62541.py
src_generated/open62541/namespace_example_generated.c: /usr/share/open62541/tools/nodeset_compiler/backend_open62541_nodes.py
src_generated/open62541/namespace_example_generated.c: /usr/share/open62541/tools/nodeset_compiler/backend_open62541_datatypes.py
src_generated/open62541/namespace_example_generated.c: ../nodeset/server_nodeset.xml
src_generated/open62541/namespace_example_generated.c: /usr/share/open62541/tools/ua-nodeset/Schema/Opc.Ua.NodeSet2.xml
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --blue --bold --progress-dir=/home/ammar/open62541/examples/cmake-build-debug/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Generating ../src_generated/open62541/namespace_example_generated.c, ../src_generated/open62541/namespace_example_generated.h"
	cd /home/ammar/open62541/examples/cmake-build-debug/nodeset && /usr/bin/python /usr/share/open62541/tools/nodeset_compiler/nodeset_compiler.py --types-array=UA_TYPES --types-array=UA_TYPES --existing=/usr/share/open62541/tools/ua-nodeset/Schema/Opc.Ua.NodeSet2.xml --xml=/home/ammar/open62541/examples/nodeset/server_nodeset.xml /home/ammar/open62541/examples/cmake-build-debug/src_generated/open62541/namespace_example_generated

src_generated/open62541/namespace_example_generated.h: src_generated/open62541/namespace_example_generated.c
	@$(CMAKE_COMMAND) -E touch_nocreate src_generated/open62541/namespace_example_generated.h

src_generated/open62541/example_nodeids.h: /usr/share/open62541/tools/generate_nodeid_header.py
src_generated/open62541/example_nodeids.h: ../nodeset/server_nodeset.csv
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --blue --bold --progress-dir=/home/ammar/open62541/examples/cmake-build-debug/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Generating ../src_generated/open62541/example_nodeids.h"
	cd /home/ammar/open62541/examples/cmake-build-debug/nodeset && /usr/bin/python /usr/share/open62541/tools/generate_nodeid_header.py /home/ammar/open62541/examples/nodeset/server_nodeset.csv /home/ammar/open62541/examples/cmake-build-debug/src_generated/open62541/example_nodeids EXAMPLE_NS

nodeset/CMakeFiles/server_nodeset.dir/server_nodeset.c.o: nodeset/CMakeFiles/server_nodeset.dir/flags.make
nodeset/CMakeFiles/server_nodeset.dir/server_nodeset.c.o: ../nodeset/server_nodeset.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/ammar/open62541/examples/cmake-build-debug/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Building C object nodeset/CMakeFiles/server_nodeset.dir/server_nodeset.c.o"
	cd /home/ammar/open62541/examples/cmake-build-debug/nodeset && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/server_nodeset.dir/server_nodeset.c.o   -c /home/ammar/open62541/examples/nodeset/server_nodeset.c

nodeset/CMakeFiles/server_nodeset.dir/server_nodeset.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/server_nodeset.dir/server_nodeset.c.i"
	cd /home/ammar/open62541/examples/cmake-build-debug/nodeset && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/ammar/open62541/examples/nodeset/server_nodeset.c > CMakeFiles/server_nodeset.dir/server_nodeset.c.i

nodeset/CMakeFiles/server_nodeset.dir/server_nodeset.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/server_nodeset.dir/server_nodeset.c.s"
	cd /home/ammar/open62541/examples/cmake-build-debug/nodeset && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/ammar/open62541/examples/nodeset/server_nodeset.c -o CMakeFiles/server_nodeset.dir/server_nodeset.c.s

nodeset/CMakeFiles/server_nodeset.dir/__/src_generated/open62541/namespace_example_generated.c.o: nodeset/CMakeFiles/server_nodeset.dir/flags.make
nodeset/CMakeFiles/server_nodeset.dir/__/src_generated/open62541/namespace_example_generated.c.o: src_generated/open62541/namespace_example_generated.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/ammar/open62541/examples/cmake-build-debug/CMakeFiles --progress-num=$(CMAKE_PROGRESS_4) "Building C object nodeset/CMakeFiles/server_nodeset.dir/__/src_generated/open62541/namespace_example_generated.c.o"
	cd /home/ammar/open62541/examples/cmake-build-debug/nodeset && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/server_nodeset.dir/__/src_generated/open62541/namespace_example_generated.c.o   -c /home/ammar/open62541/examples/cmake-build-debug/src_generated/open62541/namespace_example_generated.c

nodeset/CMakeFiles/server_nodeset.dir/__/src_generated/open62541/namespace_example_generated.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/server_nodeset.dir/__/src_generated/open62541/namespace_example_generated.c.i"
	cd /home/ammar/open62541/examples/cmake-build-debug/nodeset && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/ammar/open62541/examples/cmake-build-debug/src_generated/open62541/namespace_example_generated.c > CMakeFiles/server_nodeset.dir/__/src_generated/open62541/namespace_example_generated.c.i

nodeset/CMakeFiles/server_nodeset.dir/__/src_generated/open62541/namespace_example_generated.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/server_nodeset.dir/__/src_generated/open62541/namespace_example_generated.c.s"
	cd /home/ammar/open62541/examples/cmake-build-debug/nodeset && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/ammar/open62541/examples/cmake-build-debug/src_generated/open62541/namespace_example_generated.c -o CMakeFiles/server_nodeset.dir/__/src_generated/open62541/namespace_example_generated.c.s

# Object files for target server_nodeset
server_nodeset_OBJECTS = \
"CMakeFiles/server_nodeset.dir/server_nodeset.c.o" \
"CMakeFiles/server_nodeset.dir/__/src_generated/open62541/namespace_example_generated.c.o"

# External object files for target server_nodeset
server_nodeset_EXTERNAL_OBJECTS =

bin/examples/server_nodeset: nodeset/CMakeFiles/server_nodeset.dir/server_nodeset.c.o
bin/examples/server_nodeset: nodeset/CMakeFiles/server_nodeset.dir/__/src_generated/open62541/namespace_example_generated.c.o
bin/examples/server_nodeset: nodeset/CMakeFiles/server_nodeset.dir/build.make
bin/examples/server_nodeset: /usr/lib/x86_64-linux-gnu/libopen62541.so.1.1.5
bin/examples/server_nodeset: nodeset/CMakeFiles/server_nodeset.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/ammar/open62541/examples/cmake-build-debug/CMakeFiles --progress-num=$(CMAKE_PROGRESS_5) "Linking C executable ../bin/examples/server_nodeset"
	cd /home/ammar/open62541/examples/cmake-build-debug/nodeset && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/server_nodeset.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
nodeset/CMakeFiles/server_nodeset.dir/build: bin/examples/server_nodeset

.PHONY : nodeset/CMakeFiles/server_nodeset.dir/build

nodeset/CMakeFiles/server_nodeset.dir/clean:
	cd /home/ammar/open62541/examples/cmake-build-debug/nodeset && $(CMAKE_COMMAND) -P CMakeFiles/server_nodeset.dir/cmake_clean.cmake
.PHONY : nodeset/CMakeFiles/server_nodeset.dir/clean

nodeset/CMakeFiles/server_nodeset.dir/depend: src_generated/open62541/namespace_example_generated.c
nodeset/CMakeFiles/server_nodeset.dir/depend: src_generated/open62541/namespace_example_generated.h
nodeset/CMakeFiles/server_nodeset.dir/depend: src_generated/open62541/example_nodeids.h
	cd /home/ammar/open62541/examples/cmake-build-debug && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/ammar/open62541/examples /home/ammar/open62541/examples/nodeset /home/ammar/open62541/examples/cmake-build-debug /home/ammar/open62541/examples/cmake-build-debug/nodeset /home/ammar/open62541/examples/cmake-build-debug/nodeset/CMakeFiles/server_nodeset.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : nodeset/CMakeFiles/server_nodeset.dir/depend

