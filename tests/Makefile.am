include $(top_srcdir)/config/Makefile.for.ROSE.includes.and.libs

# DQ (4/17/2010): If we are not supporting C++ then we can't run most of our tests, but if 
# we are supporting only Fortran then we can at least run the Fortran tests (same with PHP).
# SUBDIRS = CompileTests roseTests RunTests PerformanceTests CompilerOptionsTests translatorTests
# SUBDIRS = CompileTests roseTests
SUBDIRS = . CompileTests

if !ROSE_USE_CLANG_FRONTEND

#if !ROSE_USE_INSURE
# DQ (4/23/2011): Ignoring this whole directory might be over kill. But I have had problems, 
# only on Hudson, just compiling a number of these subdirectories.
   SUBDIRS += roseTests
#endif

SUBDIRS += RunTests
if ROSE_BUILD_CXX_LANGUAGE_SUPPORT
   SUBDIRS += PerformanceTests CompilerOptionsTests translatorTests
endif

endif

# Three phases (levels) of test programs:
#  Phase 1) Analysis only, compiles the original code
#  Phase 2) Analysis and code generation, compiles the original code
#  Phase 3) analysis, code generation, and compiling the generated code
bin_PROGRAMS = \
   testParser \
   testFrontEnd \
   testAnalysis \
   testCodeGeneration \
   testTranslator \
   testTranslatorUnfoldedConstants \
   testTranslatorFoldedConstants \
   testAstFileIO \
   testAstFileRead \
   parallelASTMerge \
   testGraphGeneration \
   testTokenGeneration

# Phase 0 tests code:
# This is a test of the EDG and OFP parser support. No significant part of ROSE is generated.
# A bit of the infrastructure of ROSE is tested that sets up work to build the ROSE AST, etc.
# This test only works with C/C++ and Fortran.  Not clear what it means for the binary, 
# which has its own options for skipping different phases.
testParser_SOURCES = testParser.C

# This test code just calls the EDG frontend and then compiles the 
# original source file using the backend specified to ROSE.  This is 
# useful in evaluating the newer EDG frontend against test codes.
testFrontEnd_SOURCES = testFrontEnd.C

# Phase 1 tests code:
# This is used in specialized tests outside of ROSE testing for analysis only of large projects.
# This tests only the ability to read the application, not regenerate the source code and compile it using the backend compiler.
# This test code is used for the testing of ROSE compiling ROSE to support analysis using Compass.
testAnalysis_SOURCES = testAnalysis.C

# Phase 2 tests code:
# This is used in specialized tests outside of ROSE testing (e.g. intermediate testing of ROSE compiling large projects).
# This test the ability to read the application and generate the source code, but uses the original source code 
# with the backend compiler to generate object files or executables.
testCodeGeneration_SOURCES = testCodeGeneration.C

# Phase 3 tests code (used in all internal tests of ROSE; regression tests):
# This tests the ability to read and regnerate the input application and compile it using the backend compiler.
testTranslator_SOURCES = testTranslator.C

# These tests an option to use frontend specific constant folding or the original expression trees (default is no constnt folding).
testTranslatorUnfoldedConstants_SOURCES = testTranslatorUnfoldedConstants.C
testTranslatorFoldedConstants_SOURCES   = testTranslatorFoldedConstants.C

# This tests the ability to generate a compressed binary file representation of the AST.
# This is a special format of the ROSE AST build for performance File I/O (to address the
# requirements of large scale applications and their analysis).
testAstFileIO_SOURCES = testAstFileIO.C

# This tests the ability to read one or more of the binary AST files generated from the AST write.
testAstFileRead_SOURCES = testAstFileRead.C

# This test the parallel merge of AST files using Cong's threaded implmentation of the merge infrastructure.
parallelASTMerge_SOURCES = parallelASTMerge.C

# This tests the ability to generate dot file (AST and WholeGraph) for the input application.
testGraphGeneration_SOURCES = testGraphGeneration.C

# This tests the token generation capabilities which are an option in ROSE used to support the 
# generation of patches to define transformations of large scale applications.
testTokenGeneration_SOURCES = testTokenGeneration.C

# Use names set in ROSE/config/Makefile.for.ROSE.includes.and.libs
INCLUDES = $(ROSE_INCLUDES)
LDADD    = $(LIBS_WITH_RPATH) $(ROSE_LIBS)

EXTRA_DIST = CMakeLists.txt input_codes inputFile.C tests.tex

# ********************************
# *******  Analysis Tests  *******
# ********************************
testObjectFileAnalysis: testAnalysis
if ROSE_BUILD_CXX_LANGUAGE_SUPPORT
#	DQ (4/19/2011): copy $(srcdir)/inputFile.C to intermediate file to avoid collisions in generation of rose_inputFile.C within parallel make.
	cp $(srcdir)/inputFile.C inputObjectFileAnalysis.C
#	./testAnalysis $(INCLUDES) -c $(srcdir)/inputFile.C -o alt_Analysis_inputFile.o
	./testAnalysis $(INCLUDES) -c inputObjectFileAnalysis.C -o alt_Analysis_inputFile.o
else
	@echo "SKIPPING target '$@' because the C/C++ frontend is not enabled."
endif

testLinkFileAnalysis: testObjectFileAnalysis
if ROSE_BUILD_CXX_LANGUAGE_SUPPORT
#	./testAnalysis $(INCLUDES) alt_inputFile.o -o alt_inputFile
	./testAnalysis $(INCLUDES) alt_Analysis_inputFile.o -o testLinkFileAnalysisExecutable
else
	@echo "SKIPPING target '$@' because the C/C++ frontend is not enabled."
endif

testCppFileAnalysis: testAnalysis
if ROSE_BUILD_CXX_LANGUAGE_SUPPORT
	./testAnalysis -E $(INCLUDES) $(srcdir)/inputFile.C
else
	@echo "SKIPPING target '$@' because the C/C++ frontend is not enabled."
endif

testExecutableFileAnalysis: testAnalysis
if ROSE_BUILD_CXX_LANGUAGE_SUPPORT
	cp $(srcdir)/inputFile.C inputFileAnalysis.C
	./testAnalysis $(INCLUDES) inputFileAnalysis.C -o testExecutableFileAnalysisExecutable
else
	@echo "SKIPPING target '$@' because the C/C++ frontend is not enabled."
endif

test_testAnalysis: testObjectFileAnalysis testLinkFileAnalysis testCppFileAnalysis testExecutableFileAnalysis


# ****************************************
# *******  Code Generation Tests  ********
# ****************************************
testObjectFileCodeGeneration: testCodeGeneration
if ROSE_BUILD_CXX_LANGUAGE_SUPPORT
#	DQ (4/19/2011): copy $(srcdir)/inputFile.C to intermediate file to avoid collisions in generation of rose_inputFile.C within parallel make.
	cp $(srcdir)/inputFile.C inputObjectFileCodeGeneration.C
#	./testCodeGeneration $(INCLUDES) -c $(srcdir)/inputFile.C -o alt_CodeGeneration_inputFile.o
	./testCodeGeneration $(INCLUDES) -c inputObjectFileCodeGeneration.C -o alt_CodeGeneration_inputFile.o
else
	@echo "SKIPPING target '$@' because the C/C++ frontend is not enabled."
endif

testLinkFileCodeGeneration: testObjectFileCodeGeneration
if ROSE_BUILD_CXX_LANGUAGE_SUPPORT
	./testCodeGeneration -rose:Cxx alt_CodeGeneration_inputFile.o -o testLinkFileCodeGenerationExecutable
else
	@echo "SKIPPING target '$@' because the C/C++ frontend is not enabled."
endif

testCppFileCodeGeneration: testCodeGeneration
if ROSE_BUILD_CXX_LANGUAGE_SUPPORT
	./testCodeGeneration -E $(INCLUDES) $(srcdir)/inputFile.C
else
	@echo "SKIPPING target '$@' because the C/C++ frontend is not enabled."
endif

testExecutableFileCodeGeneration: testCodeGeneration
if ROSE_BUILD_CXX_LANGUAGE_SUPPORT
	cp $(srcdir)/inputFile.C inputFileCodeGeneration.C
	./testCodeGeneration $(INCLUDES) inputFileCodeGeneration.C -o testExecutableFileCodeGenerationExecutable
else
	@echo "SKIPPING target '$@' because the C/C++ frontend is not enabled."
endif

test_testCodeGeneration: testObjectFileCodeGeneration testLinkFileCodeGeneration testCppFileCodeGeneration testExecutableFileCodeGeneration


# **********************************
# *******  Translator Tests  *******
# **********************************
testObjectFileTranslator: testTranslator
if ROSE_BUILD_CXX_LANGUAGE_SUPPORT
#	DQ (4/19/2011): copy $(srcdir)/inputFile.C to intermediate file to avoid collisions in generation of rose_inputFile.C within parallel make.
	cp $(srcdir)/inputFile.C inputObjectFileTranslator.C
#	./testTranslator $(INCLUDES) -c $(srcdir)/inputFile.C -o alt_Translator_inputFile.o
	./testTranslator $(INCLUDES) -c inputObjectFileTranslator.C -o alt_Translator_inputFile.o
else
	@echo "SKIPPING target '$@' because the C/C++ frontend is not enabled."
endif

testLinkFileTranslator: testObjectFileTranslator
if ROSE_BUILD_CXX_LANGUAGE_SUPPORT
	./testTranslator $(INCLUDES) alt_Translator_inputFile.o -o testLinkFileTranslatorExecutable
else
	@echo "SKIPPING target '$@' because the C/C++ frontend is not enabled."
endif

testCppFileTranslator: testTranslator
if ROSE_BUILD_CXX_LANGUAGE_SUPPORT
	./testTranslator -E $(INCLUDES) $(srcdir)/inputFile.C
else
	@echo "SKIPPING target '$@' because the C/C++ frontend is not enabled."
endif

testExecutableFileTranslator: testTranslator
if ROSE_BUILD_CXX_LANGUAGE_SUPPORT
	cp $(srcdir)/inputFile.C inputFileTranslator.C
	./testTranslator $(INCLUDES) inputFileTranslator.C -o testExecutableFileTranslatorExecutable
else
	@echo "SKIPPING target '$@' because the C/C++ frontend is not enabled."
endif

test_testTranslator: testObjectFileTranslator testLinkFileTranslator testCppFileTranslator testExecutableFileTranslator

# ************************************
# *******  AST File I/O Tests  *******
# ************************************
testObjectFileFileAstFileIO: testAstFileIO
if ROSE_BUILD_CXX_LANGUAGE_SUPPORT
	cp $(srcdir)/inputFile.C alt_AstFileIO_inputFile.C
	./testAstFileIO $(INCLUDES) -c alt_AstFileIO_inputFile.C -o alt_AstFileIO_inputFile
else
	@echo "SKIPPING target '$@' because the C/C++ frontend is not enabled."
endif

test_testAstFileIO: testObjectFileFileAstFileIO

# *************************************
# *******  AST File Read Tests  *******
# *************************************
testAstRead: testAstFileRead testAstFileIO
	cp $(srcdir)/inputFile.C alt_AstFileRead_inputFile.C
	./testAstFileIO -c alt_AstFileRead_inputFile.C -o alt_AstFileRead_inputFile
	./testAstFileRead alt_AstFileRead_inputFile.C outputFileName_XXX

test_testAstFileIO: testObjectFileFileAstFileIO

# Reported failure from Tristan
testSimpleLinkFileTranslator: testTranslator
if ROSE_BUILD_CXX_LANGUAGE_SUPPORT
	cp $(srcdir)/inputFile.C inputSimpleLinkFileTranslator.C
	./testTranslator inputSimpleLinkFileTranslator.C -lm
else
	@echo "SKIPPING target '$@' because the C/C++ frontend is not enabled."
endif


# *****************************************
# *******  Token Generation Tests  ********
# *****************************************
testObjectFileTokenGeneration: testTokenGeneration
#	DQ (4/19/2011): copy $(srcdir)/inputFile.C to intermediate file to avoid collisions in generation of rose_inputFile.C within parallel make.
	cp $(srcdir)/inputFile.C alt_ObjectFileTokenGeneration_inputFile.C
#	./testTokenGeneration $(INCLUDES) -c $(srcdir)/inputFile.C -o alt_TokenGeneration_inputFile.o
	./testTokenGeneration $(INCLUDES) -c alt_ObjectFileTokenGeneration_inputFile.C -o alt_TokenGeneration_inputFile.o

if ROSE_BUILD_CXX_LANGUAGE_SUPPORT
test_testTokenGeneration: testObjectFileTokenGeneration
else
test_testTokenGeneration:
	@echo "SKIPPING target '$@' because the C/C++ frontend is not enabled."
endif

if !ROSE_USE_CLANG_FRONTEND

# Rule to run all the example translator tests
# test: test_testAnalysis test_testCodeGeneration test_testTranslator test_testAstFileIO testSimpleLinkFileTranslator
test: test_testTokenGeneration test_testAnalysis test_testCodeGeneration test_testTranslator test_testAstFileIO testSimpleLinkFileTranslator testTranslatorFoldedConstants testTranslatorUnfoldedConstants

check-local: test
	@echo "************************************************************************"
	@echo "****** ROSE/tests: make check rule complete (terminated normally) ******"
	@echo "************************************************************************"

endif

clean-local: myCleanRule

myCleanRule:
	rm -f testLinkFileAnalysisExecutable testLinkFileCodeGenerationExecutable testLinkFileTranslatorExecutable testExecutableFileAstFileIOExecutable
	rm -f testExecutableFileAnalysisExecutable testExecutableFileCodeGenerationExecutable testExecutableFileTranslatorExecutable
	rm -f rose_*.C inputFileTranslator.C inputFileCodeGeneration.C inputFileAnalysis.C inputSimpleLinkFileTranslator.C inputFileAstFileIO.C
	rm -f inputObjectFileAnalysis.C inputObjectFileCodeGeneration.C inputObjectFileTranslator.C alt_ObjectFileTokenGeneration_inputFile.C
	rm -f alt_AstFileIO_inputFile* alt_AstFileRead_inputFile* a.out *.dot *.binary
	rm -f *.C_identity

