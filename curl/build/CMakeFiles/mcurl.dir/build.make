# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.16

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:


# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

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
CMAKE_COMMAND = /usr/local/bin/cmake

# The command to remove a file.
RM = /usr/local/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/larry/myproject/myc++proj/muduostd/curl

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/larry/myproject/myc++proj/muduostd/curl/build

# Include any dependencies generated for this target.
include CMakeFiles/mcurl.dir/depend.make

# Include the progress variables for this target.
include CMakeFiles/mcurl.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/mcurl.dir/flags.make

CMakeFiles/mcurl.dir/mcurl.cc.o: CMakeFiles/mcurl.dir/flags.make
CMakeFiles/mcurl.dir/mcurl.cc.o: ../mcurl.cc
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/larry/myproject/myc++proj/muduostd/curl/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object CMakeFiles/mcurl.dir/mcurl.cc.o"
	/usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/mcurl.dir/mcurl.cc.o -c /home/larry/myproject/myc++proj/muduostd/curl/mcurl.cc

CMakeFiles/mcurl.dir/mcurl.cc.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/mcurl.dir/mcurl.cc.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/larry/myproject/myc++proj/muduostd/curl/mcurl.cc > CMakeFiles/mcurl.dir/mcurl.cc.i

CMakeFiles/mcurl.dir/mcurl.cc.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/mcurl.dir/mcurl.cc.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/larry/myproject/myc++proj/muduostd/curl/mcurl.cc -o CMakeFiles/mcurl.dir/mcurl.cc.s

# Object files for target mcurl
mcurl_OBJECTS = \
"CMakeFiles/mcurl.dir/mcurl.cc.o"

# External object files for target mcurl
mcurl_EXTERNAL_OBJECTS =

bin/mcurl: CMakeFiles/mcurl.dir/mcurl.cc.o
bin/mcurl: CMakeFiles/mcurl.dir/build.make
bin/mcurl: lib/libmuduo_curl.a
bin/mcurl: CMakeFiles/mcurl.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/larry/myproject/myc++proj/muduostd/curl/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking CXX executable bin/mcurl"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/mcurl.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/mcurl.dir/build: bin/mcurl

.PHONY : CMakeFiles/mcurl.dir/build

CMakeFiles/mcurl.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/mcurl.dir/cmake_clean.cmake
.PHONY : CMakeFiles/mcurl.dir/clean

CMakeFiles/mcurl.dir/depend:
	cd /home/larry/myproject/myc++proj/muduostd/curl/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/larry/myproject/myc++proj/muduostd/curl /home/larry/myproject/myc++proj/muduostd/curl /home/larry/myproject/myc++proj/muduostd/curl/build /home/larry/myproject/myc++proj/muduostd/curl/build /home/larry/myproject/myc++proj/muduostd/curl/build/CMakeFiles/mcurl.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/mcurl.dir/depend
