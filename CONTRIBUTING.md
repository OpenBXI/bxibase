# Opening an issue

For a good bug report:

1. Search for existing Issues, both on GitHub and in general with
   Google/Stack Overflow before posting a duplicate question.
2. Update to ```master``` branch, if possible, especially if you are
   already using git. It's possible that the bug you are about to
   report has already been fixed. Eventually you can check updating to
   ```rc``` or ```develop``` branches which contain all developments
   and bug fixes for next release (for rc) or the current state (for
   develop).

When making a bug report, it is helpful to tell us as much as you can
about your system (such as bxibase version or commit if using git,
backtrace product version, Python version, OS Version, how you
built/installed , etc.)

You can fill the following form to help us fix the bug:

```
Versions
--------
 - backtrace:
 - bxibase:

Environment
-----------
 - Python:
 - Gcc/Clang/Icc:
 - Compilation options/command:

 - OS:

Issue condition
---------------
 - Code snippet:
 - Command invocation:
 - Misc (OS conditions such as charge, FS full, ...):
```

# Contributing

# Pull Requests are welcome!

We would be glad to receive your Pull Requests about a new feature
implementation or bug fix (in addition to opening an issue).

We only accept contributions on the **develop** branch, as the
*master* branch is dedicated to contain the last stable released
version.

For those who are new to Github and/or not familiar with the process,
to contribute to the project you have to
1. Fork the project with your own account
2. Do the development in the *develop* branch (according to guidelines
   described in the next section)
3. Commit and push to your own project fork
4. Create a Pull Request through the Github web interface

## Coding convention and code quality We aimed to provide the best
quality software possible, and to achieve this goal we use several
coding convention and tools, which are:
- [PEP8](http://www.python.org/dev/peps/pep-0008) (for Python)
- [PyLint](https://www.pylint.org) (for Python)
- [Cppcheck](http://cppcheck.sourceforge.net) (for C)
- [Valgrind](http://valgrind.org) (for both Python and C)
- [Coverity](https://www.synopsys.com/software-integrity/resources/datasheets/coverity.html)
  (for C)

All the specific configuration files for all these products can be
found in the *misc/shared* folder.

Also, quite all of our development follow the
[TDD](https://en.wikipedia.org/wiki/Test-driven_development) method,
ensuring a high level of code coverage by unit tests. For those not
familiar with this method the principle is to first write the test
that shows the bug, or for new features write tests first, and ensure
that the test(s) is(are) failing -- this is to ensure that the test is
working by really testing something; then code the feature or fix the
bug and pass the test(s). Other tests should still work, ensuring your
are not introducing new bugs or braking APIs.

To run the tests suite, you can invoke the ```make check``` command,
after having configured the project using the ```./bootstrap.sh &&
./configure``` commands in the source.
