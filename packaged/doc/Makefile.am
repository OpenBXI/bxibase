if HAVE_DOXYGEN

doxygen.warn:
	$(DOXYGEN) Doxyfile
	exit $$(cat $@ | wc -l)


doc:
	$(DOXYGEN) Doxyfile

all-local: doxygen.warn

clean-local:
	rm -rf html doxygen.warn

endif
