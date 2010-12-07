### Generic native Skirmish AI config
#

Find_Package(PythonLibs)

set(mySourceDirRel         "src") # Common values are "" or "src"
set(additionalSources      ${CUtils_SRC})
set(additionalCompileFlags "")
set(additionalLibraries    ${LegacyCpp_AIWRAPPER_TARGET})

if    (PYTHONLIBS_FOUND)
	include_directories(${PYTHON_INCLUDE_DIRS})
	ConfigureNativeSkirmishAI(mySourceDirRel additionalSources additionalCompileFlags additionalLibraries)
else  (PYTHONLIBS_FOUND)
	message(STATUS "  BaczekKPAI: Not building; Python not found!")
endif (PYTHONLIBS_FOUND)
