/* ====================================================================
 * The Apache Software License, Version 1.1
 *
 * Copyright (c) 2000-2001 The Apache Software Foundation.  All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The end-user documentation included with the redistribution,
 *    if any, must include the following acknowledgment:
 *       "This product includes software developed by the
 *        Apache Software Foundation (http://www.apache.org/)."
 *    Alternately, this acknowledgment may appear in the software itself,
 *    if and wherever such third-party acknowledgments normally appear.
 *
 * 4. The names "Apache" and "Apache Software Foundation" must
 *    not be used to endorse or promote products derived from this
 *    software without prior written permission. For written
 *    permission, please contact apache@apache.org.
 *
 * 5. Products derived from this software may not be called "Apache",
 *    nor may "Apache" appear in their name, without prior written
 *    permission of the Apache Software Foundation.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE APACHE SOFTWARE FOUNDATION OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Apache Software Foundation.  For more
 * information on the Apache Software Foundation, please see
 * <http://www.apache.org/>.
 */

#ifndef SMS_PRIVATE_H
#define SMS_PRIVATE_H

#include "apr.h"
#include "apr_errno.h"
#include "apr_pools.h"
#include "apr_lock.h"
#include "apr_portable.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * The memory system structure
 */
struct apr_sms_t
{
    apr_sms_t  *parent;
    apr_sms_t  *child;
    apr_sms_t  *sibling;
    apr_sms_t **ref;
    apr_sms_t  *accounting;
    const char *identity; /* a string identifying the module */

    apr_pool_t *pool;
    apr_lock_t *sms_lock;
    
    struct apr_sms_cleanup *cleanups;

    void * (*malloc_fn)            (apr_sms_t *sms, apr_size_t size);
    void * (*calloc_fn)            (apr_sms_t *sms, apr_size_t size);
    void * (*realloc_fn)           (apr_sms_t *sms, void *memory, 
                                    apr_size_t size);
    apr_status_t (*free_fn)        (apr_sms_t *sms, void *memory);
    apr_status_t (*reset_fn)       (apr_sms_t *sms);
    apr_status_t (*pre_destroy_fn) (apr_sms_t *sms);
    apr_status_t (*destroy_fn)     (apr_sms_t *sms);
    apr_status_t (*lock_fn)        (apr_sms_t *sms);
    apr_status_t (*unlock_fn)      (apr_sms_t *sms);

    apr_status_t (*apr_abort)(int retcode);
    struct apr_hash_t *prog_data;

#if APR_HAS_THREADS    
    apr_status_t (*thread_register_fn)   (apr_sms_t *sms, 
                                          apr_os_thread_t thread);
    apr_status_t (*thread_unregister_fn) (apr_sms_t *sms, 
                                          apr_os_thread_t thread);
    apr_uint16_t threads;
#endif /* APR_HAS_THREADS */

#if APR_DEBUG_TAG_SMS
    const char *tag;
#endif
};

/*
 * private memory system functions
 */

/**
 * Initialize a memory system
 * @caution Call this function as soon as you have obtained a block of memory
 *          to serve as a memory system structure from your 
 *          apr_xxx_sms_create. Only use this function when you are
 *          implementing a memory system.
 * @param sms The memory system created
 * @param parent_sms The parent memory system
 * @deffunc apr_status_t apr_sms_init(apr_sms_t *sms,
 *                                    apr_sms_t *parent_sms)
 */
APR_DECLARE(apr_status_t) apr_sms_init(apr_sms_t *sms, 
                                       apr_sms_t *parent_sms);

/**
 * Do post init work that needs the sms to have been fully
 * initialised.
 * @param sms The memory system to use
 */
APR_DECLARE(apr_status_t) apr_sms_post_init(apr_sms_t *sms);


#ifdef __cplusplus
}
#endif

#endif /* !SMS_PRIVATE_H */

