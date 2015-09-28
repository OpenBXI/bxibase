all-local:
	mkdir -p report

if HAVE_PYTHON
check-local: 
	PYTHONPATH=${abs_top_srcdir}/packaged/lib:${abs_top_builddir}/packaged/lib/:$${PYTHONPATH} \
		LD_LIBRARY_PATH=${abs_top_builddir}/packaged/lib/.libs:$${LD_LIBRARY_PATH} \
		nosetests -w ${top_srcdir} ${NOSETESTS_ARGS}
endif

TESTS = unit_t
check_PROGRAMS= unit_t
unit_t_CFLAGS =\
			   -I$(top_srcdir)/packaged/include\
			   -I$(top_srcdir)/packaged/src\
			   $(ZMQ_CFLAGS)\
			   $(UUID_CFLAGS)\
			   @TST_CFLAGS@

unit_t_LDFLAGS =\
				@TST_LDFLAGS@\
				$(ZMQ_LIBS)\
				$(UUID_LIBS)
				
unit_t_LDADD=$(top_builddir)/packaged/lib/libbxibase.la\
				   @TST_LIBS@


#TESTS_ENVIRONMENT=@VALGRIND@ @VALGRIND_ARGS@
AUTOMAKE_OPTIONS = parallel-tests
TEST_EXTENSIONS = .pl .sh
LOG_COMPILER =${VALGRIND}  `if   test "${VALGRIND}" !=  ""   ; then echo "${VALGRIND_ARGS}"; fi`
LOG_DRIVER=$(top_srcdir)/custom-test-driver

compile_tests:$(check_PROGRAMS)

unit_t$(EEXT):force

force:${top_srcdir}/valgrind.supp
	mkdir -p report
	-[ -f $$(basename $^)  ] || cp $^ .

unit_t_SOURCES=\
			   test_mem.c\
			   test_err.c\
			   test_time.c\
			   test_logger.c\
			   unit_t.c

DISTCLEANFILES=\
			   valgrind.supp\
			   ${PACKAGE_NAME}.bxilog\
			   unit_t.bxilog\
			   lt-unit_t.bxilog\
			   report/${PACKAGE_NAME}-Results.xml\
			   report/${PACKAGE_NAME}-Listing.xml
