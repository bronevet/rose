include $(top_srcdir)/config/Makefile.for.ROSE.includes.and.libs

# ------------------------------------------------------------------------------
#  Globals
# ------------------------------------------------------------------------------

COMPASS2=$(top_builddir)/projects/compass2/compass2/compass2
CHECKER=$(COMPASS2)
COMPASS_PARAMETERS_XSD=$(top_srcdir)/projects/compass2/compass2/xml/compass_parameters.xsd
TEST_SCRIPT=$(top_srcdir)/projects/compass2/tests/checkers/test_checker.sh

TESTCODES=\
	keyword_macro_test_1.cpp

# ------------------------------------------------------------------------------
#  Test rules
# ------------------------------------------------------------------------------

$(TESTCODES:.cpp=.o): $(TESTCODES)
	$(TEST_SCRIPT) \
		$(CHECKER) \
		$(srcdir)/$(@:.o=.cpp) \
		"Keyword macro detected."

$(COMPASS2):
	$(MAKE) -C $(top_builddir)/projects/compass2

check-local: $(COMPASS2)
	$(MAKE) $(TESTCODES:.cpp=.o)

# ------------------------------------------------------------------------------
#
# ------------------------------------------------------------------------------

clean-local:
	rm -f \
		rose_*.cpp \
		*.o \
		*.out \
		*.ti

