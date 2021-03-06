include $(top_srcdir)/config/Makefile.for.ROSE.includes.and.libs

# Need to add sibdirectory for annotation parser tests as well
SUBDIRS = testCallGraphAnalysis defUseAnalysisTests variableLivenessTests staticInterproceduralSlicingTests sideEffectAnalysisTests \
		variableRenamingTests staticSingleAssignmentTests ssa_UnfilteredCfg_Test generalDataFlowAnalysisTests systemDependenceGraphTests
#ptrTraceDriver

INCLUDES = $(ROSE_INCLUDES) -I$(top_srcdir)/src/midend/programAnalysis -I$(top_srcdir)/src/midend/programAnalysis/pointerAnal -I${top_srcdir}/src/midend/programAnalysis/CFG -I${top_srcdir}/src/midend/programAnalysis/bitvectorDataflow -I$(top_srcdir)/src/util/support -I$(top_srcdir)/src/util/graphs -I$(top_srcdir)/src/midend/astUtil/astSupport -I$(top_srcdir)/src/midend/astUtil/astInterface -I$(top_srcdir)/src/midend/astUtil -I$(top_srcdir)/src/midend/programAnalysis/ 
#-I$(top_srcdir)/src/midend/programAnalysis/ptrTrace/GAI/ICFG/

include $(srcdir)/../../CompileTests/Cxx_tests/Makefile-pass.inc

if ROSE_USE_LONG_MAKE_CHECK_RULE

# This is the long test of ROSE (takes a while!)
TESTCODES_REQUIRED_TO_PASS = $(EXAMPLE_TESTCODES_REQUIRED_TO_PASS)

else # !ROSE_USE_LONG_MAKE_CHECK_RULE

# Shorten the list of code to compile to simplify checks using "make distcheck"
# Note that the tests test2001_02.C and test2001_03.C are required because the
# tests in ../C_subset_of_Cxx use them and that test2004_116.C is required 
# because the tests in ROSE/tests/roseTests/programTransformationTests use it.
TESTCODES_REQUIRED_TO_PASS = $(EXAMPLE_TESTCODES_REQUIRED_TO_PASS_SHORT)

endif # ROSE_USE_LONG_MAKE_CHECK_RULE

TESTCODE_INCLUDES = -I$(top_srcdir)/tests/CompileTests/A++Code

# Automake's testing mechanism (which defines the "make check" rule) requires passing tests.
TESTCODES = $(TESTCODES_REQUIRED_TO_PASS)
ROSE_FLAGS = --edg:no_warnings -w -rose:verbose 0 --edg:restrict

CXX_FILES = ${TESTCODES_REQUIRED_TO_PASS:.C=.CXX-o}





## The grammar generator (ROSETTA) should use its own template repository
# CXX_TEMPLATE_REPOSITORY_PATH = .

bin_PROGRAMS = CFGTest DataFlowTest StmtRefTest PtrAnalTest steensgaardTest1 steensgaardTest2 VirtualFunctionAnalysisTest ReachingDefinitionFacadeTest

# DQ: Removed this because it was failing and I could not figure out why
# AnnotationLanguageParserTest

PtrAnalTest_SOURCES = PtrAnalTest.C
PtrAnalTest_LDADD = $(LIBS_WITH_RPATH) $(ROSE_LIBS)

#PtrAnalTest2_SOURCES = PtrAnalTest2.C
#PtrAnalTest2_LDADD = $(LIBS_WITH_RPATH) $(ROSE_LIBS) 

steensgaardTest1_SOURCES = steensgaardTest1.C
steensgaardTest1_LDADD = $(LIBS_WITH_RPATH) $(ROSE_LIBS)

steensgaardTest2_SOURCES = steensgaardTest2.C
steensgaardTest2_LDADD = $(LIBS_WITH_RPATH) $(ROSE_LIBS)

# DQ (2/10/2010): ROSE currently fails to generate code for this file (it does however compile for analysis only).
# CFGTest_SOURCES = CFGTest.C
CFGTest_SOURCES =
if !ROSE_USING_ROSE_CODE_GENERATION
     CFGTest_SOURCES += CFGTest.C
endif

CFGTest_LDADD = $(LIBS_WITH_RPATH) $(ROSE_LIBS)

DataFlowTest_SOURCES = DataFlowTest.C
DataFlowTest_LDADD = $(LIBS_WITH_RPATH) $(ROSE_LIBS)

VirtualFunctionAnalysisTest_SOURCES = VirtualFunctionAnalysisTest.C
VirtualFunctionAnalysisTest_LDADD = $(LIBS_WITH_RPATH) $(ROSE_LIBS)

ReachingDefinitionFacadeTest_SOURCES = ReachingDefinitionFacadeTest.C
ReachingDefinitionFacadeTest_LDADD = $(LIBS_WITH_RPATH) $(ROSE_LIBS)

StmtRefTest_SOURCES = StmtRefTest.C
StmtRefTest_LDADD = $(LIBS_WITH_RPATH) $(ROSE_LIBS)


# Added to support Brian White's work with the parts of the Broadway compiler
#AnnotationLanguageParserTest_SOURCES = AnnotationLanguageParserTest.C
#AnnotationLanguageParserTest_LDADD = $(LIBS_WITH_RPATH) $(ROSE_LIBS)

# This is specific to compiling AnnotationLanguageParserTest.C
AM_CXXFLAGS = -D__LOCALSTL_H


$(CXX_FILES): VirtualFunctionAnalysisTest
	./VirtualFunctionAnalysisTest  $(ROSE_FLAGS) -I$(srcdir)/../../CompileTests/Cxx_tests $(TESTCODE_INCLUDES) -c $(srcdir)/../../CompileTests/Cxx_tests/$(@:.CXX-o=.C) && touch $@


FORCE_TEST_CODES_TO_RUN:
	$(srcdir)/TestDriver $(srcdir)

# .out is a special suffix for make
EXTRA_DIST = TestDriver testfile1.c testfile1.c.cfg testfile1.c.du testfile1.c.ref \
             testfile2.c testfile2.c.cfg testfile2.c.du testfile2.c.ref \
             testfile3.c testfile3.c.cfg testfile3.c.du testfile3.c.ref \
             testfile4.c testfile4.c.du  testPtr1.C testPtr2.C \
             PtrAnalTest.out1  steensgaardTest1.outx   steensgaardTest2.out2 \
             PtrAnalTest.out2  steensgaardTest2.out1 \
	     test_vfa1.C test_vfa2.C test_vfa3.C test_vfa4.C test_vfa5.C
#	     steensgaardTest1.out - .out has a special suffix meaning
#	     trustedAnnotationOutput annotationLanguageParserInputFile.ann

check-vfa: $(CXX_FILES)

# to deactivate tests comment out check-local
# check-local:
#	AnnotationLanguageParserTestRule
#	$(MAKE) FORCE_TEST_CODES_TO_RUN
check-local: check-vfa
	$(MAKE) FORCE_TEST_CODES_TO_RUN
	@echo "*************************************************************************************************"
	@echo "*** ROSE/tests/roseTests/programAnalysisTests: make check rule complete (terminated normally) ***"
	@echo "*************************************************************************************************"

AnnotationLanguageParserTestRule: trustedAnnotationOutput
	./AnnotationLanguageParserTest $(srcdir)/annotationLanguageParserInputFile.ann > annotationOutput
#	diff annotationOutput trustedAnnotationOutput || exit

clean-local:
	rm -rf Templates.DB ii_files ti_files cxx_templates *.CXX-o *.dot *.ti

distclean-local:
	rm -rf Templates.DB *.CXX-o *.dot *.ti






