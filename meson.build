###########
# project #
###########
project('notstd', 'c', version: '0.0.8')

cc = meson.get_compiler('c')

type = 'library'

###########
# include #
###########

includeDir = include_directories('include')

###############
# source file #
###############

src  = [ 'src/core/core.c' ]
src += [ 'src/core/err.c' ]
src += [ 'src/core/math.c' ]

src += [ 'src/time/delay.c' ]

src += [ 'src/memory/page.c' ]
src += [ 'src/memory/memory.c' ]
src += [ 'src/memory/protect.c' ]
src += [ 'src/memory/extras.c' ]

src += [ 'src/concurrency/futex.c' ]

src += [ 'src/datastructure/vector.c' ]
src += [ 'src/datastructure/list.c' ]
src += [ 'src/datastructure/hashalg.c' , 'src/datastructure/rbhash.c' ]
src += [ 'src/datastructure/fzs.c' ]
src += [ 'src/datastructure/phq.c' ]

#src += [ 'src/str.c', 'src/utf8.c', 'src/itoa.c', 'src/regex.c' ] #, 'src/rules.c' ]
#src += [ 'src/serialize.c', 'src/customjson.c', 'src/ajson.c' ]
#src += [ 'src/rbhash.c', 'src/hashalg.c' ]
#src += [ 'src/list.c' ]
#src += [ 'src/phq.c' ]
#src += [ 'src/trie.c' ]
#src += [ 'src/fzs.c' ]
#src += [ 'src/bipbuffer.c' ]
#src += [ 'src/lbuffer.c' ]
#src += [ 'src/threads.c' ]
#src += [ 'src/process.c' ]
#src += [ 'src/path.c', 'src/dir.c', 'src/fd.c', 'src/file.c', 'src/request.c', 'src/fgeneric.c', 'src/f.c' ]

ut = get_option('ut')
if ut == 'memory' 
  message('testing memory')
  src += [ 'test/src/memory.c' ]
  type = 'executable'
  add_global_arguments('-DMEMORY_DEBUG=1', language: 'c')
elif ut == 'delay'
  message('testing delay')
  src += [ 'test/src/delay.c' ]
  type = 'executable'
  add_global_arguments('-DDELAY_DEBUG=1', language: 'c')
elif ut == 'datastructure'
  message('testing datastructure')
  src += [ 'test/src/datastructure.c' ]
  src += [ 'test/src/vector.c' ]
  src += [ 'test/src/list.c' ]
  src += [ 'test/src/hash.c' ]
  src += [ 'test/src/fzs.c' ]
  src += [ 'test/src/phq.c' ]
  type = 'executable'
  add_global_arguments('-DDS_DEBUG=1', language: 'c')
  add_global_arguments('-DTEST_DATASTRUCTURE=@0@'.format(get_option('utvalue')), language: 'c')
endif

##############
# data files #
##############

#############
# man files #
#############

##################
# compiler flags #
##################

# warnings
add_global_arguments('-Wall', language: 'c')
add_global_arguments('-Wextra', language: 'c')
add_global_arguments('-Wuninitialized', language: 'c')
add_global_arguments('-Winit-self', language: 'c')
add_global_arguments('-Wstrict-aliasing', language: 'c')
add_global_arguments('-Wstrict-overflow', language: 'c')
add_global_arguments('-Wfloat-equal', language: 'c')
add_global_arguments('-Wvla', language: 'c')

# unicode var name
#add_global_arguments('-finput-charset=UTF-8', language: 'c')
add_global_arguments('-fextended-identifiers', language: 'c')

#reentrant malloc
add_global_arguments('-pthread', language: 'c')
add_global_link_arguments('-pthread', language:'c')

# standard
add_global_arguments('-std=gnu2x', language: 'c')

# open mp
if get_option('openmp') > 0
  message('openmp enabled')
  add_global_arguments('-fopenmp', language:'c')
  add_global_link_arguments('-fopenmp', language:'c')
  add_global_arguments('-DOMP_ENABLE=1', language: 'c')
endif 

# gprof
if get_option('gprof') > 0
  add_global_arguments('-pg', language:'c')
  add_global_link_arguments('-pg', language:'c')
endif

# optimization
if( get_option('optimize') == 3 )
  message('agressive optimization enabled')
  add_global_arguments('-O3', language: 'c')
  add_global_arguments('-march=native', language: 'c')
  add_global_arguments('-mtune=native', language: 'c')
elif( get_option('optimize') == 2 )
  message('local optimization enabled')
  add_global_arguments('-O2', language: 'c')
  add_global_arguments('-march=native', language: 'c')
  add_global_arguments('-mtune=native', language: 'c')
elif( get_option('optimize') == 1 )
  message('generic optimization enabled')
  add_global_arguments('-O2', language: 'c')
else
   add_global_arguments('-O0', language: 'c') 
  add_global_arguments('-g', language: 'c')
endif

if get_option('autovectorization') > 0
  if( get_option('optimize') < 1 )
	error('required option optimize > 0')
  endif
  message('vectorization enabled')
  add_global_arguments('-ftree-vectorize', language:'c')
  add_global_arguments('-DVECTORIZE=1', language:'c')
endif

##########################
# libraries dependencies #
##########################
libDeps  = [ cc.find_library('m', required : true) ]       # math
#libDeps += [ cc.find_library('rt', required : true) ]      # shared mem
#libDeps += [ cc.find_library('dl', required : true) ]      # dynamic load
#libDeps += [ cc.find_library('uring', required : true) ]   # iouring
#libDeps += [ cc.find_library('pcre2-8', required : true) ] # regex perl

#########################
# software dependencies #
#########################

#################
# Custom Target #
#################

#########
# debug #
#########


opt_debug = get_option('ebug')
if opt_debug > 0
  message('debug enabled @0@'.format(opt_debug))
  add_global_arguments('-DDBG_ENABLE=@0@'.format(opt_debug), language: 'c')
  #add_global_arguments('-ftree-vectorizer-verbose=5', language:'c')
  #add_global_arguments('-fopt-info-loop-optimized', language:'c')
  #add_global_arguments('-fopt-info-vec-optimized', language:'c')
  #add_global_arguments('-fopt-info-vec-missed', language:'c')
endif

if get_option('olor') > 0
  message('color debug enabled')
  add_global_arguments('-DDBG_COLOR=1', language: 'c')
endif

if get_option('shortpath') > 0
  message('shortpath debug enabled')
  add_global_arguments('-DDBG_SHORTPATH=1', language: 'c')
endif

if get_option('assert') > 0
  message('assertion enabled')
  add_global_arguments('-DASSERT_ENABLE=1', language: 'c')
endif

###########
# license #
###########

message('copyright vbextreme 2018')
message('released under GPLv3')

#########
# build #
#########

if type == 'executable' 
  executable(meson.project_name(), src, include_directories: includeDir, dependencies: libDeps, install: true)
else
  shared_library(meson.project_name(), src, include_directories: includeDir, dependencies: libDeps, install: true)
endif















