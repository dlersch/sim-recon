
import os
import subprocess
import SCons
import glob
import re

#===========================================================
# The first 3 sections provide routines for building a
# library, program, or plugin from all the source in the
# current directory. The routines that follow add support
# for various packages
#===========================================================


##################################
# library
##################################
def library(env, libname=''):

	# Library name comes from directory name
	if libname=='':
		libname = os.path.split(os.getcwd())[1]

	env.PrependUnique(CPPPATH = ['.'])

	# Add C/C++, and FORTRAN targets
	env.AppendUnique(ALL_SOURCES = env.Glob('*.c*'))
	env.AppendUnique(ALL_SOURCES = env.Glob('*.F'))

	sources = env['ALL_SOURCES']

	# Build static library from all source
	myobjs = env.Object(sources)
	mylib = env.Library(target = libname, source = myobjs)

	# Cleaning and installation are restricted to the directory
	# scons was launched from or its descendents
	CurrentDir = env.Dir('.').srcnode().abspath
	if not CurrentDir.startswith(env.GetLaunchDir()):
		# Not in launch directory. Tell scons not to clean these targets
		env.NoClean([myobjs, mylib])
	else:
		# We're in launch directory (or descendent) schedule installation

		# Installation directories for library and headers
		installdir = env.subst('$INSTALLDIR')
		includedir = "%s/%s" %(env.subst('$INCDIR'), libname)
		libdir = env.subst('$LIBDIR')

		# Install targets 
		env.Install(libdir, mylib)
		env.Install(includedir, env.Glob('*.h*'))


##################################
# executable
##################################
def executable(env, exename=''):

	# Executable name comes from directory name
	if exename=='':
		exename = os.path.split(os.getcwd())[1]

	env.PrependUnique(CPPPATH = ['.'])

	# Add C/C++, and FORTRAN targets
	env.AppendUnique(ALL_SOURCES = env.Glob('*.c*'))
	env.AppendUnique(ALL_SOURCES = env.Glob('*.F'))

	sources = env['ALL_SOURCES']

	# Build program from all source
	myobjs = env.Object(sources)
	myexe = env.Program(target = exename, source = myobjs)

	# Cleaning and installation are restricted to the directory
	# scons was launched from or its descendents
	CurrentDir = env.Dir('.').srcnode().abspath
	if not CurrentDir.startswith(env.GetLaunchDir()):
		# Not in launch directory. Tell scons not to clean these targets
		env.NoClean([myobjs, myexe])
	else:
		# We're in launch directory (or descendent) schedule installation

		# Installation directories for executable and headers
		installdir = env.subst('$INSTALLDIR')
		includedir = env.subst('$INCDIR')
		bindir = env.subst('$BINDIR')

		# Install targets 
		env.Install(bindir, myexe)


##################################
# executables
##################################
def executables(env):

	# This will generate multiple executables from the
	# source in the current directory. It does this
	# by identifying source files that define "main()"
	# and linking those with all source files that do not
	# define "main()". Program names are based on the 
	# filename of the source file defining "main()"
	main_sources = []
	common_sources = []
	curpath = os.getcwd()
	srcpath = env.Dir('.').srcnode().abspath
	os.chdir(srcpath)
	for f in glob.glob('*.c*'):
		if 'main(' in open(f).read():
			main_sources.append(f)
		else:
			common_sources.append(f)

	for f in glob.glob('*.F'):
		if '      PROGRAM ' in open(f).read():
			main_sources.append(f)
		else:
			common_sources.append(f)
	os.chdir(curpath)
	
	env.PrependUnique(CPPPATH = ['.'])

	common_sources.extend(env['ALL_SOURCES'])

	# Build program from all source
	main_objs = env.Object(main_sources)
	common_objs = env.Object(common_sources)

	progs = []
	for obj in main_objs:
		exename = re.sub('\.o$', '', str(obj))  # strip off ".o" from object file name
		progs.append(env.Program(target = exename, source = [obj, common_objs]))

	# Cleaning and installation are restricted to the directory
	# scons was launched from or its descendents
	CurrentDir = env.Dir('.').srcnode().abspath
	if not CurrentDir.startswith(env.GetLaunchDir()):
		# Not in launch directory. Tell scons not to clean these targets
		env.NoClean([common_objs, main_objs, progs])
	else:
		# We're in launch directory (or descendent) schedule installation
		bindir = env.subst('$BINDIR')
		env.Install(bindir, progs)


##################################
# plugin
##################################
def plugin(env, pluginname=''):

	# Library name comes from directory name
	if pluginname=='':
		pluginname = os.path.split(os.getcwd())[1]

	env.PrependUnique(CPPPATH = ['.'])

	# Add C/C++ targets
	env.AppendUnique(ALL_SOURCES = env.Glob('*.c*'))

	sources = env['ALL_SOURCES']

	# Build static library from all source
	myobjs = env.SharedObject(sources)
	myplugin = env.SharedLibrary(target = pluginname, source = myobjs, SHLIBPREFIX='', SHLIBSUFFIX='.so')

	# Cleaning and installation are restricted to the directory
	# scons was launched from or its descendents
	CurrentDir = env.Dir('.').srcnode().abspath
	if not CurrentDir.startswith(env.GetLaunchDir()):
		# Not in launch directory. Tell scons not to clean these targets
		env.NoClean([myobjs, myplugin])
	else:
		# We're in launch directory (or descendent) schedule installation

		# Installation directories for plugin and headers
		installdir = env.subst('$INSTALLDIR')
		includedir = "%s/%s" %(env.subst('$INCDIR'), pluginname)
		pluginsdir = env.subst('$PLUGINSDIR')

		# Install targets 
		installed = env.Install(pluginsdir, myplugin)
		env.Install(includedir, env.Glob('*.h*'))




#===========================================================
# Package support follows
#===========================================================


##################################
# JANA
##################################
def AddJANA(env):
	JANA_CFLAGS = subprocess.Popen(["jana-config", "--cflags"], stdout=subprocess.PIPE).communicate()[0]
	JANA_LINKFLAGS = subprocess.Popen(["jana-config", "--libs"], stdout=subprocess.PIPE).communicate()[0]

	AddCompileFlags(env, JANA_CFLAGS)
	AddLinkFlags(env, JANA_LINKFLAGS)


##################################
# HDDS
##################################
def AddHDDS(env):
	hdds_home = os.getenv('HDDS_HOME', 'hdds')
	env.AppendUnique(CPPPATH = ["%s/src" % hdds_home])
	env.AppendUnique(LIBPATH = ["%s/lib/%s" % (hdds_home, env['OSNAME'])])


##################################
# HDDM
##################################
def AddHDDM(env):
	env.AppendUnique(LIBS = 'HDDM')


##################################
# DANA
##################################
def AddDANA(env):
	AddJANA(env)
	AddCCDB(env)
	AddHDDS(env)
	AddXERCES(env)
	Add_xstream(env)
	DANA_LIBS  = "DANA ANALYSIS PID TAGGER TRACKING START_COUNTER"
	DANA_LIBS += " CERE RICH CDC TRIGGER"
	DANA_LIBS += " FDC TOF BCAL FCAL CCAL HDGEOMETRY HDDM JANA"
	env.PrependUnique(LIBS = DANA_LIBS.split())

##################################
# xstream
##################################
def Add_xstream(env):
	env.AppendUnique(CPPPATH = ['#external/xstream/include'])
	env.AppendUnique(CCFLAGS = ['-fPIC'])
	env.AppendUnique(LIBS=['xstream', 'bz2', 'z'])


##################################
# CCDB
##################################
def AddCCDB(env):
	ccdb_home = os.getenv('CCDB_HOME', 'ccdb')
	CCDB_CPPPATH = "%s/include" % (ccdb_home)
	CCDB_LIBPATH = "%s/lib" % (ccdb_home)
	CCDB_LIBS = "ccdb"
	env.AppendUnique(CPPPATH = CCDB_CPPPATH)
	env.AppendUnique(LIBPATH = CCDB_LIBPATH)
	env.AppendUnique(LIBS    = CCDB_LIBS)


##################################
# Xerces
##################################
def AddXERCES(env):
	xercescroot = os.getenv('XERCESCROOT', 'xerces')
	XERCES_CPPPATH = "%s/include" % (xercescroot)
	XERCES_LIBPATH = "%s/lib" % (xercescroot)
	XERCES_LIBS = "xerces-c"
	env.AppendUnique(CPPPATH = XERCES_CPPPATH)
	env.AppendUnique(LIBPATH = XERCES_LIBPATH)
	env.AppendUnique(LIBS    = XERCES_LIBS)


##################################
# CERNLIB
##################################
def AddCERNLIB(env):
	env.PrependUnique(FORTRANFLAGS = ['-ffixed-line-length-0', '-fno-second-underscore'])
	env.PrependUnique(FORTRANFLAGS = ['-fno-automatic'])
	env.PrependUnique(FORTRANPATH = ['include'])
	cern = os.getenv('CERN', '/usr/local/cern/PRO')
	cern_level = os.getenv('CERN_LEVEL', '2006')
	cern_root = '%s/%s' % (cern, cern_level)
	CERN_FORTRANPATH = "%s/include" % cern_root
	CERN_LIBPATH = "%s/lib" % cern_root
	env.AppendUnique(FORTRANPATH   = [CERN_FORTRANPATH])
	env.AppendUnique(CPPPATH   = CERN_FORTRANPATH)
	env.AppendUnique(LIBPATH   = CERN_LIBPATH)
	env.AppendUnique(LINKFLAGS = ['-rdynamic'])
	env.AppendUnique(LIBS      = ['gfortran', 'geant321', 'pawlib', 'lapack3', 'blas', 'graflib', 'grafX11', 'packlib', 'mathlib', 'kernlib', 'X11', 'nsl', 'crypt', 'dl'])
	env.SetOption('warn', 'no-fortran-cxx-mix')  # supress warnings about linking fortran with c++


##################################
# ROOT
##################################
def AddROOT(env):
	#
	# Here we use the root-config program to give us the compiler
	# and linker options needed for ROOT. We use the AddCompileFlags()
	# and AddLinkFlags() routines (defined below) to split the arguments
	# into the categories scons wants. E.g. scons wants to know the
	# search path and basenames for libraries rather than just giving it
	# the full compiler options like "-L/path/to/lib -lmylib".
	#
	# We also create a builder for ROOT dictionaries and add targets to
	# build dictionaries for any headers with "ClassDef" in them.

	ROOT_CFLAGS = subprocess.Popen(["root-config", "--cflags"], stdout=subprocess.PIPE).communicate()[0]
	ROOT_LINKFLAGS = subprocess.Popen(["root-config", "--glibs"], stdout=subprocess.PIPE).communicate()[0]
	AddCompileFlags(env, ROOT_CFLAGS)
	AddLinkFlags(env, ROOT_LINKFLAGS)
	env.AppendUnique(LIBS = "Geom")

	# Create Builder that can convert .h file into _Dict.cc file
	rootsys = os.getenv('ROOTSYS', '/usr/local/root/PRO')
	env.AppendENVPath('LD_LIBRARY_PATH', '%s/lib' % rootsys )
	if env['SHOWBUILD']==0:
		rootcintaction = SCons.Script.Action("%s/bin/rootcint -f $TARGET -c $SOURCE" % (rootsys), 'ROOTCINT   [$SOURCE]')
	else:
		rootcintaction = SCons.Script.Action("%s/bin/rootcint -f $TARGET -c $SOURCE" % (rootsys))
	bld = SCons.Script.Builder(action = rootcintaction, suffix='_Dict.cc', src_suffix='.h')
	env.Append(BUILDERS = {'ROOTDict' : bld})
	env.Append(LD_LIBRARY_PATH = os.environ['LD_LIBRARY_PATH'])

	# Generate ROOT dictionary file targets for each header
	# containing "ClassDef"
	#
	# n.b. It seems if scons is run when the build directory doesn't exist,
	# then the cwd is set to the source directory. Otherwise, it is the
	# build directory. Since the headers will only exist in the source
	# directory, we must temporarily cd into that to look for headers that
	# we wish to generate dictionaries for. (This took a long time to figure
	# out!)
	curpath = os.getcwd()
	srcpath = env.Dir('.').srcnode().abspath
	if(int(env['SHOWBUILD'])>1):
		print "---- Scanning for headers to generate ROOT dictionaries in: %s" % srcpath
	os.chdir(srcpath)
	for f in glob.glob('*.h*'):
		if 'ClassDef' in open(f).read():
			env.AppendUnique(ALL_SOURCES = env.ROOTDict(f))
			if(int(env['SHOWBUILD'])>1):
				print "       ROOT dictionary for %s" % f
	os.chdir(curpath)



#===========================================================
# Misc utility routines follow
#===========================================================

##################################
# AddCompileFlags
##################################
def AddCompileFlags(env, allflags):

	# The allflags parameter should be a string containing all
	# of the link flags (e.g. what is returned by root-config --cflags)
	# It is split on white space and the parameters sorted into
	# the 2 lists: ccflags, cpppath

	ccflags = []
	cpppath = []
	for f in allflags.split():
		if f.startswith('-I'):
			cpppath.append(f[2:])
		else:
			ccflags.append(f)
	
	if len(ccflags)>0 :
		env.AppendUnique(CCFLAGS=ccflags)

	if len(cpppath)>0 :
		env.AppendUnique(CPPPATH=cpppath)


##################################
# AddLinkFlags
##################################
def AddLinkFlags(env, allflags):

	# The allflags parameter should be a string containing all
	# of the link flags (e.g. what is returned by root-config --glibs)
	# It is split on white space and the parameters sorted into
	# the 3 lists: linkflags, libpath, and libs

	linkflags = []
	libpath = []
	libs = []
	for f in allflags.split():
		if f.startswith('-L'):
			libpath.append(f[2:])
		elif f.startswith('-l'):
			libs.append(f[2:])
		else:
			linkflags.append(f)

	if len(linkflags)>0 :
		env.AppendUnique(LINKFLAGS=linkflags)

	if len(libpath)>0 :
		env.AppendUnique(LIBPATH=libpath)
		
	if len(libs)>0 :
		env.AppendUnique(LIBS=libs)


