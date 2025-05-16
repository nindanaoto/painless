project('painless', 'cpp',
  version : '0.1',
  default_options : ['warning_level=2', 'cpp_std=c++20'])
fs = import('fs')


# Compiler
cpp = meson.get_compiler('cpp')

# Dependencies
mpi_dep = dependency('mpi', language : 'cpp', required : true)
openmp_dep = dependency('openmp', required : true)
thread_dep = dependency('threads', required : true)
zlib_dep = dependency('zlib', required : true)
boost_dep = dependency('boost', required : true) # Added Boost
# Math library (libm) is usually linked automatically or by the C/C++ compiler dependency.
# We can add it explicitly if needed:
# math_dep = cpp.find_library('m', required : false)

# Include directories
eigen_inc = include_directories('libs/eigen-3.4.0')
# m4ri include will be from its build directory or source after configure
# For now, let's assume headers are in libs/m4ri-20200125 after configure/build
m4ri_inc_src = include_directories('libs/m4ri-20200125') # This might need adjustment

# Source files for the main painless executable
painless_src_dir = 'src'
painless_sources = []
# Find all .cpp files in src directory and its subdirectories
# Exclude any potential .ignore paths if necessary (not seen in current Makefile)
src_files_find = run_command('find', painless_src_dir, '-name', '*.cpp', check: true)
painless_sources = src_files_find.stdout().strip().split('\n')

# --- m4ri library ---
# The Makefile runs: cd libs/m4ri-20200125 && autoreconf --install && ./configure --enable-thread-safe && make
# We'll use a custom target to build m4ri
m4ri_dir = meson.current_source_dir() / 'libs' / 'm4ri-20200125'
m4ri_lib_path = m4ri_dir / '.libs' / 'libm4ri.a' # Path after make



# We need to declare a dependency on the built m4ri library
# The path to the library will be meson.current_build_dir() / 'libm4ri.a'
# However, custom_target output handling for libraries is tricky.
# A simpler approach for now might be to build it and then link directly by path.
# Let's refine this part later if direct linking by path from custom_target output is problematic.
# For now, we'll assume the custom_target makes it available.
# The Makefile links it from libs/m4ri-20200125/.libs/libm4ri.a
# So, if the custom_target just runs make in that dir, the lib is there.




m4ri_built_lib_name = 'libm4ri.a'
# m4ri_target_output_dir = meson.current_build_dir() / 'libs_build' # No longer needed for this structure
# m4ri_built_lib_path = m4ri_target_output_dir / m4ri_built_lib_name # No longer needed for this structure
m4ri_final_lib_destination_in_builddir = meson.current_build_dir() / m4ri_built_lib_name

m4ri_compile_target = custom_target('m4ri_compile',
  output : m4ri_built_lib_name, # The library file itself, output to builddir/
  command : [
    'sh', '-c',
    'cd @0@ && autoreconf -fi && ./configure --enable-thread-safe CFLAGS="-fPIC" CXXFLAGS="-fPIC" && make && cp .libs/libm4ri.a @1@'.format(
      m4ri_dir,
      m4ri_final_lib_destination_in_builddir
    )
  ],
  build_by_default : true,
  # install : false # Not installing system-wide, just making it available for this project
)
m4ri_dep = declare_dependency(
  link_with : m4ri_compile_target,
  include_directories : m4ri_inc_src # Headers are in the source dir of m4ri
)


# --- Solvers ---
# We will add custom_target for each solver similar to m4ri or as per Makefile
# For now, let's list them and prepare for their integration.

# Yalsat (dependency for Lingeling)
yalsat_dir = meson.current_source_dir() / 'solvers' / 'yalsat'
yalsat_built_lib_name = 'libyals.a'
# yalsat_target_output_dir = meson.current_build_dir() / 'solvers_build' / 'yalsat' # Removed
yalsat_built_lib_path = meson.current_build_dir() / yalsat_built_lib_name # Path for lingeling to find it, and for this target's cp

yalsat_compile_target = custom_target('yalsat_compile',
  output : yalsat_built_lib_name, # Output will be builddir/libyals.a
  command : [
    'sh', '-c',
    # Command copies libyals.a from yalsat_dir to yalsat_built_lib_path (which is builddir/libyals.a)
    'cd @0@ && bash ./configure.sh && make && cp libyals.a @1@'.format(
      yalsat_dir,
      yalsat_built_lib_path # Destination for cp is builddir/libyals.a
    )
  ],
  build_by_default : true,
)
yalsat_dep = declare_dependency(
  link_with : yalsat_compile_target,
  include_directories : include_directories('solvers/yalsat') # Assuming headers are in yalsat_dir
)
# Example for Lingeling (needs configure.sh and make)
lingeling_dir = meson.current_source_dir() / 'solvers' / 'lingeling'
lingeling_built_lib_name = 'liblgl.a'
# lingeling_target_output_dir = meson.current_build_dir() / 'solvers_build' / 'lingeling' # Removed
# lingeling_built_lib_path = lingeling_target_output_dir / lingeling_built_lib_name # Old definition, effectively replaced
lingeling_final_lib_destination_in_builddir = meson.current_build_dir() / lingeling_built_lib_name

lingeling_compile_target = custom_target('lingeling_compile',
  output : lingeling_built_lib_name, # Output will be builddir/liblgl.a
  command : [
    'sh', '-c',
    # yalsat_built_lib_path is builddir/libyals.a, used as source for copy
    # lingeling_final_lib_destination_in_builddir is builddir/liblgl.a, used as dest for copy
    'cd @0@ && mkdir -p ../yalsat && cp @1@ ../yalsat/libyals.a && ./configure.sh && make liblgl.a && cp liblgl.a @2@'.format(
      lingeling_dir,
      yalsat_built_lib_path, # Source for libyals.a copy by lingeling configure
      lingeling_final_lib_destination_in_builddir # Destination for liblgl.a copy
    )
  ],
  build_by_default : true,
  depends : yalsat_compile_target, # Added dependency
)
lingeling_dep = declare_dependency(
  link_with : lingeling_compile_target,
  include_directories : include_directories('solvers/lingeling')
)
# Glucose Solver
glucose_dir = meson.current_source_dir() / 'solvers' / 'glucose'
glucose_built_lib_name = 'libglucose.a'
glucose_final_lib_destination_in_builddir = meson.current_build_dir() / glucose_built_lib_name
glucose_compile_target = custom_target('glucose_compile',
  output : glucose_built_lib_name,
  command : [
    'sh', '-c',
    'cd @0@ && make clean && make parallel/libglucose.a CXXFLAGS="-fPIC -O3" && cp parallel/libglucose.a @1@'.format(
      glucose_dir,
      glucose_final_lib_destination_in_builddir
    )
  ],
  build_by_default : true,
)
glucose_inc = include_directories('solvers/glucose/mtl', 'solvers/glucose/utils', 'solvers/glucose/core', 'solvers/glucose/simp')
glucose_dep = declare_dependency(
  link_with : glucose_compile_target,
  include_directories : glucose_inc
)
# Minisat Solver
minisat_dir = meson.current_source_dir() / 'solvers' / 'minisat'
minisat_built_lib_name = 'libminisat.a'
minisat_build_subdir_in_minisat_dir = minisat_dir / 'build' / 'release' / 'lib' # Path within Minisat's own build structure
minisat_original_built_lib_path = minisat_build_subdir_in_minisat_dir / minisat_built_lib_name

minisat_final_lib_destination_in_builddir = meson.current_build_dir() / minisat_built_lib_name

minisat_compile_target = custom_target('minisat_compile',
  output : minisat_built_lib_name,
  command : [
    'sh', '-c',
    'cd @0@ && make clean && make lr && mkdir -p @3@ && cp @1@ @2@'.format(
      minisat_dir,
      minisat_original_built_lib_path,
      minisat_final_lib_destination_in_builddir,
      fs.parent(minisat_final_lib_destination_in_builddir) # Ensure destination directory exists
    )
  ],
  build_by_default : true,
)

minisat_inc = include_directories(
  'solvers/minisat/minisat/mtl',
  'solvers/minisat/minisat/core',
  'solvers/minisat/minisat/simp',
  'solvers/minisat/minisat/utils'
)
minisat_dep = declare_dependency(
  link_with : minisat_compile_target,
  include_directories : minisat_inc
)
# Kissat INC Solver
kissat_inc_solver_dir = meson.current_source_dir() / 'solvers' / 'kissat-inc'
kissat_inc_solver_built_lib_name = 'libkissat_inc.a' # Renamed for clarity in build dir
kissat_inc_solver_build_subdir = kissat_inc_solver_dir / 'build'
kissat_inc_solver_original_built_lib_path = kissat_inc_solver_build_subdir / kissat_inc_solver_built_lib_name

kissat_inc_solver_final_lib_destination_in_builddir = meson.current_build_dir() / kissat_inc_solver_built_lib_name

kissat_inc_solver_compile_target = custom_target('kissat_inc_solver_compile',
  output : kissat_inc_solver_built_lib_name,
  command : [
    'sh', '-c',
    'cd @0@ && ./configure --no-proofs && make && mkdir -p @3@ && cp @1@ @2@'.format(
      kissat_inc_solver_dir,
      kissat_inc_solver_original_built_lib_path,
      kissat_inc_solver_final_lib_destination_in_builddir,
      fs.parent(kissat_inc_solver_final_lib_destination_in_builddir)
    )
  ],
  build_by_default : true,
)

kissat_inc_solver_inc = include_directories('solvers/kissat-inc')
kissat_inc_solver_dep = declare_dependency(
  link_with : kissat_inc_solver_compile_target,
  include_directories : kissat_inc_solver_inc
)
# Kissat MAB Solver
kissat_mab_solver_dir = meson.current_source_dir() / 'solvers' / 'kissat_mab'
kissat_mab_solver_built_lib_name = 'libkissat_mab.a'
kissat_mab_solver_build_subdir = kissat_mab_solver_dir / 'build'
kissat_mab_solver_original_built_lib_path = kissat_mab_solver_build_subdir / kissat_mab_solver_built_lib_name
kissat_mab_solver_final_lib_destination_in_builddir = meson.current_build_dir() / kissat_mab_solver_built_lib_name

kissat_mab_solver_compile_target = custom_target('kissat_mab_solver_compile',
  input : [], 
  output : kissat_mab_solver_built_lib_name,
  command : [
    'bash', '-c',
    'cd @0@ && ./configure --no-proofs && make && mkdir -p @1@ && cp @2@ @3@'.format(
      kissat_mab_solver_dir,
      fs.parent(kissat_mab_solver_final_lib_destination_in_builddir),
      kissat_mab_solver_original_built_lib_path,
      kissat_mab_solver_final_lib_destination_in_builddir
    )
  ],
  build_by_default : true,
)

kissat_mab_solver_inc = include_directories('solvers/kissat_mab') 
kissat_mab_solver_dep = declare_dependency(
  link_with : kissat_mab_solver_compile_target,
  include_directories : kissat_mab_solver_inc
)
# MapleCOMSPS Solver
maplecomsps_solver_dir = meson.current_source_dir() / 'solvers' / 'mapleCOMSPS'
maplecomsps_solver_built_lib_name = 'libmapleCOMSPS.a'
maplecomsps_solver_build_subdir = maplecomsps_solver_dir / 'build' / 'release' / 'lib'
maplecomsps_solver_original_built_lib_path = maplecomsps_solver_build_subdir / maplecomsps_solver_built_lib_name
maplecomsps_solver_final_lib_destination_in_builddir = meson.current_build_dir() / maplecomsps_solver_built_lib_name

maplecomsps_solver_compile_target = custom_target('maplecomsps_solver_compile',
  input : [], 
  output : maplecomsps_solver_built_lib_name,
  command : [
    'bash', '-c',
    'cd @0@ && make r && mkdir -p @1@ && cp @2@ @3@'.format(
      maplecomsps_solver_dir,
      fs.parent(maplecomsps_solver_final_lib_destination_in_builddir),
      maplecomsps_solver_original_built_lib_path,
      maplecomsps_solver_final_lib_destination_in_builddir
    )
  ],
  build_by_default : true,
)

maplecomsps_solver_inc = include_directories('solvers/mapleCOMSPS')
maplecomsps_solver_dep = declare_dependency(
  link_with : maplecomsps_solver_compile_target,
  include_directories : maplecomsps_solver_inc
)
# TaSSAT Solver
tassat_solver_dir = meson.current_source_dir() / 'solvers' / 'tassat'
tassat_solver_built_lib_name = 'libtas.a'
tassat_solver_original_built_lib_path = tassat_solver_dir / tassat_solver_built_lib_name
tassat_solver_final_lib_destination_in_builddir = meson.current_build_dir() / tassat_solver_built_lib_name

tassat_solver_compile_target = custom_target('tassat_solver_compile',
  input : [], 
  output : tassat_solver_built_lib_name,
  command : [
    'bash', '-c',
    'cd @0@ && bash ./configure.sh && make && mkdir -p @1@ && cp @2@ @3@'.format(
      tassat_solver_dir,
      fs.parent(tassat_solver_final_lib_destination_in_builddir),
      tassat_solver_original_built_lib_path,
      tassat_solver_final_lib_destination_in_builddir
    )
  ],
  build_by_default : true,
)

tassat_solver_inc = include_directories('solvers/tassat')
tassat_solver_dep = declare_dependency(
  link_with : tassat_solver_compile_target,
  include_directories : tassat_solver_inc
)
# Kissat (vanilla) Solver
kissat_solver_dir = meson.current_source_dir() / 'solvers' / 'kissat'
kissat_solver_built_lib_name = 'libkissat.a'
kissat_solver_build_subdir = kissat_solver_dir / 'build'
kissat_solver_original_built_lib_path = kissat_solver_build_subdir / kissat_solver_built_lib_name
kissat_solver_final_lib_destination_in_builddir = meson.current_build_dir() / kissat_solver_built_lib_name

kissat_solver_compile_target = custom_target('kissat_solver_compile',
  input : [], 
  output : kissat_solver_built_lib_name,
  command : [
    'bash', '-c',
    'cd @0@ && bash ./configure --no-proofs && make && mkdir -p @1@ && cp @2@ @3@'.format(
      kissat_solver_dir,
      fs.parent(kissat_solver_final_lib_destination_in_builddir),
      kissat_solver_original_built_lib_path,
      kissat_solver_final_lib_destination_in_builddir
    )
  ],
  build_by_default : true,
)

kissat_solver_inc = include_directories('solvers/kissat')
kissat_solver_dep = declare_dependency(
  link_with : kissat_solver_compile_target,
  include_directories : kissat_solver_inc
)
# CaDiCaL Solver
cadical_solver_dir = meson.current_source_dir() / 'solvers' / 'cadical'
cadical_solver_built_lib_name = 'libcadical.a'
cadical_solver_build_subdir = cadical_solver_dir / 'build'
cadical_solver_original_built_lib_path = cadical_solver_build_subdir / cadical_solver_built_lib_name
cadical_solver_final_lib_destination_in_builddir = meson.current_build_dir() / cadical_solver_built_lib_name

cadical_solver_compile_target = custom_target('cadical_solver_compile',
  input : [], 
  output : cadical_solver_built_lib_name,
  command : [
    'bash', '-c',
    'cd @0@ && bash ./configure && make && mkdir -p @1@ && cp @2@ @3@'.format(
      cadical_solver_dir,
      fs.parent(cadical_solver_final_lib_destination_in_builddir),
      cadical_solver_original_built_lib_path,
      cadical_solver_final_lib_destination_in_builddir
    )
  ],
  build_by_default : true,
)

cadical_solver_inc = include_directories('solvers/cadical')
cadical_solver_dep = declare_dependency(
  link_with : cadical_solver_compile_target,
  include_directories : cadical_solver_inc
)

# Placeholder for other solver dependencies
# minisat_dep, glucose_dep, kissat_dep, etc.
# For now, we'll only include m4ri and lingeling to test the concept.

all_solver_deps = [yalsat_dep, lingeling_dep, glucose_dep, minisat_dep, kissat_inc_solver_dep, kissat_mab_solver_dep, maplecomsps_solver_dep, tassat_solver_dep, kissat_solver_dep, cadical_solver_dep] # Add other solver deps here

# Source files for the library (excluding painless.cpp)
painless_lib_sources = []
foreach src_file : painless_sources
  if src_file != painless_src_dir / 'painless.cpp'
    painless_lib_sources += src_file
  endif
endforeach

# Painless library
painless_lib = library('painless_lib',
  painless_lib_sources,
  include_directories : [eigen_inc, m4ri_inc_src, 'src', include_directories('src/containers'), include_directories('src/preprocessors'), include_directories('src/sharing'), include_directories('src/solvers'), include_directories('src/utils'), include_directories('src/working'), include_directories('solvers'), include_directories('solvers/glucose'), include_directories('solvers/minisat')],
  dependencies : [mpi_dep, openmp_dep, thread_dep, zlib_dep, boost_dep, m4ri_dep] + all_solver_deps,
  install : true
)

# Main executable
painless_exe_sources = [painless_src_dir / 'painless.cpp']
executable('painless',
  painless_exe_sources,
  include_directories : [eigen_inc, m4ri_inc_src, 'src', include_directories('src/containers'), include_directories('src/preprocessors'), include_directories('src/sharing'), include_directories('src/solvers'), include_directories('src/utils'), include_directories('src/working'), include_directories('solvers'), include_directories('solvers/glucose'), include_directories('solvers/minisat')], # Add all necessary src subdirectories and specific solver paths.
  dependencies : [mpi_dep, openmp_dep, thread_dep, zlib_dep, boost_dep, m4ri_dep] + all_solver_deps,
  link_with : painless_lib,
  # Add MPI compile and link arguments
  # cpp_args : mpi_dep.get_compile_args(), # Already handled by dependency object in newer Meson
  # link_args : mpi_dep.get_link_args(), # Already handled by dependency object
  install : true
)

# TODO:
# 1. Add custom_targets for all other solvers based on the Makefile.
#    - Minisat: make -C solvers/minisat
#    - Glucose: cd solvers/glucose && make parallel/libglucose.a
#    - Kissat variants: cd <dir> && ./configure --no-proofs && make
#    - YalSat, TaSSAT, Cadical: cd <dir> && ./configure (or .sh) && make
#    - MapleCOMSPS: make -C solvers/mapleCOMSPS r
# 2. Ensure all necessary include directories for solvers are added.
# 3. Test compilation and linking.
# 4. Refine custom_target definitions if needed for robustness (e.g., handling of output files, dependencies between targets).
# 5. Add -lpthread, -lz, -lm if not automatically handled by dependencies.
#    (zlib_dep handles -lz, thread_dep handles -lpthread, m is usually auto)