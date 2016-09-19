BXI Base Library                                   {#index}
=====================

[TOC]

BXI Base Library for C Language                    {#BXIBase4C}
================================

## Overview ##                                           {#Overview_c}


The BXI Base Library is the basic library for most BXI projects. 
It provides the following plain C modules:

- mem.h: memory allocation and release
- str.h: string manipulation
- err.h: error handling
- time.h: timing related functions with error handling
- zmq.h: easy wrapper around ZeroMQ functions
- log.h: high performance logging library

## BXI General API Conventions ##                        {#Conventions_c}

### Naming ### {#Naming}

- All BXI C functions are prefixed by `bxi`.
- Enumerations format: `bxi<module>_name_e` 
    (see `::bxilog_level_e`)
- Structures format: `bxi<module>_name_s`. These structures are generally not used directly but through pointers
     such as: 
    `typedef struct bxi<module>_name_s bxi_<module>_name_p;` 
    (see [Object like API](#OO_c)).

### Memory allocation ###                                       {#Allocation_c}
Use bximem_calloc() for memory allocation, which provides the following guarantees:

1. Returned memory is nullified.
2. The underlying returned code is checked.

Use the BXIFREE() macro for releasing a pointer, which provides the following guarantees:

1. Releasing a NULL pointer causes no problem.
2. The pointer is nullified.

Example:

    struct mystruct * s = bximem_calloc(sizeof(*s));
    // do something with 's'
    BXIFREE(s);         // release 's'
    assert(NULL == s);  // This is always true


### Module Initialization ###                                   {#Initialization_c}
Module initialization, when required, is done using `bxi*_init()`.
Library cleanup is done using `bxi*_finalize()`. See bxilog_init() and bxilog_finalize() for example.

### Object like API ###                                         {#OO_c}
Object like module provides `bxi*_new()` and `bxi*_destroy()` functions. 
See for example: bxizmq_zocket_connect() and bxizmq_zocket_destroy().

Note: `bxi*_destroy()` 
actually *nullify* the pointer as shown in the following example:

    void * result;
    bxierr_p err = bxizmq_zocket_new(..., &result);
    ...
    // Do something with result
    bxizmq_zocket_destroy(&result);
    assert(NULL == result);  // This is true

Objects are always referred to through pointers and 
their type name always ends with a '_p' to clearly state it. See for example:
`::bxierr_p`, `::bxilog_logger_p`. Therefore, instantiating, using and destroying a BXI object always look like the following code snippet: 

    bxifoo_p object = bxifoo_new(...);
    ...
    result_p result = bxifoo_function(object, ...);
    ...
    bxifoo_destroy(&object);

### Error handling ###                                          {#Errors_c}
Dealing with errors in C is known to be difficult. The BXI base library provides 
the err.h module that helps a lot for this. The following convention holds 
for most BXI functions:

- Functions that deal with errors internally (using for example, assert() internally, 
  or error() ) use a traditional signature. See for example:
  bximem_calloc(), bxistr_new(), bxilog_get_level().
  
- Functions that ask the caller to deal with the error (most functions actually),
  return a `::bxierr_p` instance. If they must also provide a result, 
  a *pointer* on the result is taken as the *last* argument. See for example: 
  `bxizmq_zocket_connect()`, `bxitime_get()`.

BXI Base Library for Python Language               {#BXIBase4Python}
=====================================

## Overview ##                                           {#Overview_py}

The BXI Base Library also provides a binding for the Python language, 
implemented using the C Foreign Function Interface (CFFI) for Python.

Since Python provides a very rich set of modules, only the following modules are provided 
as Python binding:


| BXI Base C Module | BXI Base Python Module | Description                      |
| ----------------- | -----------------------| -------------------------------- |
| err.h             | bxi.base.err           | Error Handling                   |
| log.h             | bxi.base.log           | High Performance Logging module  |





