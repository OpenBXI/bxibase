/* -*- coding: utf-8 -*-  */

#ifndef BXILOG_CONFIG_H_
#define BXILOG_CONFIG_H_

#ifndef BXICFFI
#include <stdbool.h>
#endif

#include "bxi/base/mem.h"
#include "bxi/base/err.h"

#include "bxi/base/log.h"
#include "bxi/base/log/handler.h"
// *********************************************************************************
// ********************************** Defines **************************************
// *********************************************************************************

// *********************************************************************************
// ********************************** Types   **************************************
// *********************************************************************************


typedef struct {
    size_t handlers_nb;
    int data_hwm;
    int ctrl_hwm;
    size_t tsd_log_buf_size;
    const char * progname;
    bxilog_handler_p * handlers;
    bxilog_handler_param_p * handlers_params;
} bxilog_param_s;

typedef bxilog_param_s * bxilog_param_p;


// *********************************************************************************
// ********************************** Global Variables *****************************
// *********************************************************************************



// *********************************************************************************
// ********************************** Interface ************************************
// *********************************************************************************

bxierr_p bxilog_basic_config(const char *progname,
                             const char * filename,
                             bool append,
                             bxilog_param_p *result);

bxierr_p bxilog_unit_test_config(const char *progname,
                                 const char * filename,
                                 bool append,
                                 bxilog_param_p *result);

bxilog_param_p bxilog_param_new(const char * progname);
void bxilog_param_add(bxilog_param_p self, bxilog_handler_p handler, ...);

bxierr_p bxilog_param_destroy(bxilog_param_p * param_p);


#endif /* BXILOG_H_ */
