if HAVE_DOXYGEN

doxyfile.stamp:
	$(DOXYGEN) Doxyfile
	touch $@


doc:
	$(DOXYGEN) Doxyfile

all-local: doxyfile.stamp

clean-local:
	rm -rf html

endif
