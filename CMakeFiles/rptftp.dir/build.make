# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.23

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

#Suppress display of executed commands.
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
RM = /usr/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/bktiel/CLionProjects/rptftp

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/bktiel/CLionProjects/rptftp

# Include any dependencies generated for this target.
include CMakeFiles/rptftp.dir/depend.make
# Include any dependencies generated by the compiler for this target.
include CMakeFiles/rptftp.dir/compiler_depend.make

# Include the progress variables for this target.
include CMakeFiles/rptftp.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/rptftp.dir/flags.make

CMakeFiles/rptftp.dir/src/conn_map.c.o: CMakeFiles/rptftp.dir/flags.make
CMakeFiles/rptftp.dir/src/conn_map.c.o: src/conn_map.c
CMakeFiles/rptftp.dir/src/conn_map.c.o: CMakeFiles/rptftp.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/bktiel/CLionProjects/rptftp/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building C object CMakeFiles/rptftp.dir/src/conn_map.c.o"
	$(CMAKE_COMMAND) -E __run_co_compile --tidy="/usr/bin/clang-tidy;--checks=*,             -llvm-include-order,             -cppcoreguidelines-*,             -altera-struct-pack-align,             -fsanitize=thread            -android-cloexec-open,             -hicpp-signed-bitwise,             -readability-magic-numbers,             -readability-function-cognitive-complexity,             -clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling,             -llvmlibc-restrict-system-libc-headers,             -hiccp-no-assembler;--extra-arg-before=--driver-mode=gcc" --source=/home/bktiel/CLionProjects/rptftp/src/conn_map.c -- /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -MD -MT CMakeFiles/rptftp.dir/src/conn_map.c.o -MF CMakeFiles/rptftp.dir/src/conn_map.c.o.d -o CMakeFiles/rptftp.dir/src/conn_map.c.o -c /home/bktiel/CLionProjects/rptftp/src/conn_map.c

CMakeFiles/rptftp.dir/src/conn_map.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/rptftp.dir/src/conn_map.c.i"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/bktiel/CLionProjects/rptftp/src/conn_map.c > CMakeFiles/rptftp.dir/src/conn_map.c.i

CMakeFiles/rptftp.dir/src/conn_map.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/rptftp.dir/src/conn_map.c.s"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/bktiel/CLionProjects/rptftp/src/conn_map.c -o CMakeFiles/rptftp.dir/src/conn_map.c.s

CMakeFiles/rptftp.dir/src/main.c.o: CMakeFiles/rptftp.dir/flags.make
CMakeFiles/rptftp.dir/src/main.c.o: src/main.c
CMakeFiles/rptftp.dir/src/main.c.o: CMakeFiles/rptftp.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/bktiel/CLionProjects/rptftp/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Building C object CMakeFiles/rptftp.dir/src/main.c.o"
	$(CMAKE_COMMAND) -E __run_co_compile --tidy="/usr/bin/clang-tidy;--checks=*,             -llvm-include-order,             -cppcoreguidelines-*,             -altera-struct-pack-align,             -fsanitize=thread            -android-cloexec-open,             -hicpp-signed-bitwise,             -readability-magic-numbers,             -readability-function-cognitive-complexity,             -clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling,             -llvmlibc-restrict-system-libc-headers,             -hiccp-no-assembler;--extra-arg-before=--driver-mode=gcc" --source=/home/bktiel/CLionProjects/rptftp/src/main.c -- /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -MD -MT CMakeFiles/rptftp.dir/src/main.c.o -MF CMakeFiles/rptftp.dir/src/main.c.o.d -o CMakeFiles/rptftp.dir/src/main.c.o -c /home/bktiel/CLionProjects/rptftp/src/main.c

CMakeFiles/rptftp.dir/src/main.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/rptftp.dir/src/main.c.i"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/bktiel/CLionProjects/rptftp/src/main.c > CMakeFiles/rptftp.dir/src/main.c.i

CMakeFiles/rptftp.dir/src/main.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/rptftp.dir/src/main.c.s"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/bktiel/CLionProjects/rptftp/src/main.c -o CMakeFiles/rptftp.dir/src/main.c.s

CMakeFiles/rptftp.dir/src/networking.c.o: CMakeFiles/rptftp.dir/flags.make
CMakeFiles/rptftp.dir/src/networking.c.o: src/networking.c
CMakeFiles/rptftp.dir/src/networking.c.o: CMakeFiles/rptftp.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/bktiel/CLionProjects/rptftp/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Building C object CMakeFiles/rptftp.dir/src/networking.c.o"
	$(CMAKE_COMMAND) -E __run_co_compile --tidy="/usr/bin/clang-tidy;--checks=*,             -llvm-include-order,             -cppcoreguidelines-*,             -altera-struct-pack-align,             -fsanitize=thread            -android-cloexec-open,             -hicpp-signed-bitwise,             -readability-magic-numbers,             -readability-function-cognitive-complexity,             -clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling,             -llvmlibc-restrict-system-libc-headers,             -hiccp-no-assembler;--extra-arg-before=--driver-mode=gcc" --source=/home/bktiel/CLionProjects/rptftp/src/networking.c -- /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -MD -MT CMakeFiles/rptftp.dir/src/networking.c.o -MF CMakeFiles/rptftp.dir/src/networking.c.o.d -o CMakeFiles/rptftp.dir/src/networking.c.o -c /home/bktiel/CLionProjects/rptftp/src/networking.c

CMakeFiles/rptftp.dir/src/networking.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/rptftp.dir/src/networking.c.i"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/bktiel/CLionProjects/rptftp/src/networking.c > CMakeFiles/rptftp.dir/src/networking.c.i

CMakeFiles/rptftp.dir/src/networking.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/rptftp.dir/src/networking.c.s"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/bktiel/CLionProjects/rptftp/src/networking.c -o CMakeFiles/rptftp.dir/src/networking.c.s

CMakeFiles/rptftp.dir/src/tftp.c.o: CMakeFiles/rptftp.dir/flags.make
CMakeFiles/rptftp.dir/src/tftp.c.o: src/tftp.c
CMakeFiles/rptftp.dir/src/tftp.c.o: CMakeFiles/rptftp.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/bktiel/CLionProjects/rptftp/CMakeFiles --progress-num=$(CMAKE_PROGRESS_4) "Building C object CMakeFiles/rptftp.dir/src/tftp.c.o"
	$(CMAKE_COMMAND) -E __run_co_compile --tidy="/usr/bin/clang-tidy;--checks=*,             -llvm-include-order,             -cppcoreguidelines-*,             -altera-struct-pack-align,             -fsanitize=thread            -android-cloexec-open,             -hicpp-signed-bitwise,             -readability-magic-numbers,             -readability-function-cognitive-complexity,             -clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling,             -llvmlibc-restrict-system-libc-headers,             -hiccp-no-assembler;--extra-arg-before=--driver-mode=gcc" --source=/home/bktiel/CLionProjects/rptftp/src/tftp.c -- /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -MD -MT CMakeFiles/rptftp.dir/src/tftp.c.o -MF CMakeFiles/rptftp.dir/src/tftp.c.o.d -o CMakeFiles/rptftp.dir/src/tftp.c.o -c /home/bktiel/CLionProjects/rptftp/src/tftp.c

CMakeFiles/rptftp.dir/src/tftp.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/rptftp.dir/src/tftp.c.i"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/bktiel/CLionProjects/rptftp/src/tftp.c > CMakeFiles/rptftp.dir/src/tftp.c.i

CMakeFiles/rptftp.dir/src/tftp.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/rptftp.dir/src/tftp.c.s"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/bktiel/CLionProjects/rptftp/src/tftp.c -o CMakeFiles/rptftp.dir/src/tftp.c.s

# Object files for target rptftp
rptftp_OBJECTS = \
"CMakeFiles/rptftp.dir/src/conn_map.c.o" \
"CMakeFiles/rptftp.dir/src/main.c.o" \
"CMakeFiles/rptftp.dir/src/networking.c.o" \
"CMakeFiles/rptftp.dir/src/tftp.c.o"

# External object files for target rptftp
rptftp_EXTERNAL_OBJECTS =

bin/rptftp: CMakeFiles/rptftp.dir/src/conn_map.c.o
bin/rptftp: CMakeFiles/rptftp.dir/src/main.c.o
bin/rptftp: CMakeFiles/rptftp.dir/src/networking.c.o
bin/rptftp: CMakeFiles/rptftp.dir/src/tftp.c.o
bin/rptftp: CMakeFiles/rptftp.dir/build.make
bin/rptftp: lib/rplib/librplib.a
bin/rptftp: CMakeFiles/rptftp.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/bktiel/CLionProjects/rptftp/CMakeFiles --progress-num=$(CMAKE_PROGRESS_5) "Linking C executable bin/rptftp"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/rptftp.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/rptftp.dir/build: bin/rptftp
.PHONY : CMakeFiles/rptftp.dir/build

CMakeFiles/rptftp.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/rptftp.dir/cmake_clean.cmake
.PHONY : CMakeFiles/rptftp.dir/clean

CMakeFiles/rptftp.dir/depend:
	cd /home/bktiel/CLionProjects/rptftp && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/bktiel/CLionProjects/rptftp /home/bktiel/CLionProjects/rptftp /home/bktiel/CLionProjects/rptftp /home/bktiel/CLionProjects/rptftp /home/bktiel/CLionProjects/rptftp/CMakeFiles/rptftp.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/rptftp.dir/depend

