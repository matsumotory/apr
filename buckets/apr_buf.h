/* ====================================================================
 * The Apache Software License, Version 1.1
 *
 * Copyright (c) 2000 The Apache Software Foundation.  All rights
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

#ifndef AP_BUF_H
#define AP_BUF_H

#include "apr_mmap.h"
#include "apr_errno.h"
#include "apr_private.h"
#include "../../../include/ap_iol.h"
#ifdef HAVE_SYS_UIO_H
#include <sys/uio.h>	/* for struct iovec */
#endif
#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif


/* The basic concept behind bucket_brigades.....
 *
 * A bucket brigade is simply a Queue of buckets, where we aren't limited
 * to inserting at the front and removing at the end.
 *
 * Buckets are just data stores.  They can be files, mmap areas, or just
 * pre-allocated memory.  The point of buckets is to store data.  Along with
 * that data, come some functions to access it.  The functions are relatively
 * simple, read, write, getlen, split, and free.
 *
 * read reads a string of data.  Currently, it assumes we read all of the 
 * data in the bucket.  This should be changed to only read the specified 
 * amount.
 *
 * getlen gets the number of bytes stored in the bucket.
 * 
 * write writes the specified data to the bucket.  Depending on the type of
 * bucket, this may append to the end of previous data, or wipe out the data
 * currently in the bucket.  rwmem buckets append currently, all others 
 * erase the current bucket.
 *
 * split just makes one bucket into two at the spefied location.  To implement
 * this correctly, we really need to implement reference counting.
 *
 * free just destroys the data associated with the bucket.
 *
 * We may add more functions later.  There has been talk of needing a stat,
 * which would probably replace the getlen.  And, we definately need a convert
 * function.  Convert would make one bucket type into another bucket type.
 *
 * To write a bucket brigade, they are first made into an iovec, so that we
 * don't write too little data at one time.  Currently we ignore compacting the
 * buckets into as few buckets as possible, but if we really want to be
 * performant, then we need to compact the buckets before we convert to an
 * iovec, or possibly while we are converting to an iovec.
 *
 * I'm not really sure what else to say about the buckets.  They are relatively
 * simple and straight forward IMO.  It is just a way to organize data in
 * memory that allows us to modify that data and move it around quickly and
 * easily.
 */

typedef enum {
    AP_BUCKET_rwmem,
    AP_BUCKET_rmem,
    AP_BUCKET_file,
    AP_BUCKET_mmap,
    AP_BUCKET_filename,
    AP_BUCKET_cached_entity,
    AP_BUCKET_URI,
    AP_BUCKET_eos        /* End-of-stream bucket.  Special case to say this is
                          * the end of the bucket so all data should be sent
                          * immediately. */
} ap_bucket_color_e;

typedef struct ap_bucket ap_bucket;
struct ap_bucket {
    ap_bucket_color_e color;              /* what type of bucket is it */
    void *data;				  /* for use by free() */

    /* All of the function pointers that can act on a bucket. */
    void (*free)(void *e);                /* can be NULL */
    int (*getlen)(ap_bucket *e);          /* Get the length of the string */

    /* Read the data from the bucket. */
    const char *(*read)(ap_bucket *e);  /* Get the string */

    /* Write into a bucket.  The buf is a different type based on the
     * bucket type used.  For example, with AP_BUCKET_mmap it is an ap_mmap_t
     * for AP_BUCKET_file it is an ap_file_t, and for AP_BUCKET_rwmem it is
     * a char *.  The nbytes is the amount of actual data in buf.  This is
     * not the sizeof(buf), it is the actual number of bytes in the char *
     * that buf resolves to.  written is how much of that data was inserted
     * into the bucket.
     */ 
    int (*write)(ap_bucket *e, const void *buf, ap_size_t nbytes, ap_ssize_t *w);
   
    /* Split one bucket into to at the specified position */
    ap_status_t (*split)(ap_bucket *e, ap_size_t nbytes);

    ap_bucket *next;                     /* The next node in the bucket list */
    ap_bucket *prev;                     /* The prev node in the bucket list */
};

typedef struct ap_bucket_brigade ap_bucket_brigade;
struct ap_bucket_brigade {
    ap_pool_t *p;                       /* The pool to associate this with.
                                           I do not allocate out of the pool,
                                           but this lets me register a cleanup
                                           to put a limit on the brigade's 
                                           lifetime. */
    ap_bucket *head;                    /* The start of the brigade */
    ap_bucket *tail;                    /* The end of the brigade */
};

/*    ******  Different bucket types   *****/

typedef struct ap_bucket_rmem ap_bucket_rmem;
struct ap_bucket_rmem {
    size_t  alloc_len;                  /* how much was allocated */
    const void    *start;               /* Where does the actual data start
                                           in the alloc'ed block */
    const void    *end;                 /* where does the data actually end? */
};

typedef struct ap_bucket_rwmem ap_bucket_rwmem;
struct ap_bucket_rwmem {
    void    *alloc_addr;                /* Where does the data start */
    size_t  alloc_len;                  /* how much was allocated */
    void    *start;                     /* Where does the actual data start
                                           in the alloc'ed block */
    void    *end;                       /* where does the data actually end? */
};

typedef struct ap_bucket_mmap ap_bucket_mmap;
struct ap_bucket_mmap {
    void      *alloc_addr;   /* Where does the mmap start? */
    int       len;           /* The amount of data in the mmap that we are 
                              * referencing with this bucket.  This may be 
                              * smaller than the length in the data object, 
                              * but it may not be bigger.
                              */
};

/*   ******  Bucket Brigade Functions  *****  */

/* Create a new bucket brigade */
APR_EXPORT(ap_bucket_brigade *) ap_brigade_create(ap_pool_t *p);

/* destroy an enitre bucket brigade */
APR_EXPORT(ap_status_t) ap_brigade_destroy(void *b);

/* append bucket(s) to a bucket_brigade */
APR_EXPORT(void) ap_brigade_append_buckets(ap_bucket_brigade *b,
                                                  ap_bucket *e);

/* consume nbytes from beginning of b -- call ap_bucket_destroy as
    appropriate, and/or modify start on last element */
APR_EXPORT(void) ap_brigade_consume(ap_bucket_brigade *, int nbytes);

/* create an iovec of the elements in a bucket_brigade... return number 
    of elements used */
APR_EXPORT(int) ap_brigade_to_iovec(ap_bucket_brigade *, 
                                           struct iovec *vec, int nvec);

/* catenate bucket_brigade b onto bucket_brigade a, bucket_brigade b is 
    empty after this */
APR_EXPORT(void) ap_brigade_catenate(ap_bucket_brigade *a, 
                                            ap_bucket_brigade *b);

/* Destroy the first nvec buckets. */
APR_EXPORT(void) ap_consume_buckets(ap_bucket_brigade *b, int nvec);

/* save the buf out to the specified iol.  This can be used to flush the
    data to the disk, or to send it out to the network. */
APR_EXPORT(ap_status_t) ap_brigade_to_iol(ap_ssize_t *total_bytes,
                                                 ap_bucket_brigade *a, 
                                                 ap_iol *iol);

APR_EXPORT(int) ap_brigade_vputstrs(ap_bucket_brigade *b, va_list va);

APR_EXPORT(int) ap_brigade_printf(ap_bucket_brigade *b, const char *fmt, ...);

APR_EXPORT(int) ap_brigade_vprintf(ap_bucket_brigade *b, const char *fmt, va_list va);

/*   ******  Bucket Functions  *****  */

/* destroy a bucket */
APR_EXPORT(ap_status_t) ap_bucket_destroy(ap_bucket *e);

/* destroy an entire list of buckets */
APR_EXPORT(ap_status_t) ap_bucket_list_destroy(ap_bucket *e);

/* Convert a bucket to a char * */
APR_EXPORT(const char *) ap_get_bucket_char_str(ap_bucket *b);

/* get the length of the data in the bucket */
APR_EXPORT(int) ap_get_bucket_len(ap_bucket *b);

/****** Functions to Create Buckets of varying type ******/

/* Create a read/write memory bucket */
APR_EXPORT(ap_bucket *) ap_bucket_rwmem_create(const void *buf,
                                ap_size_t nbyte, ap_ssize_t *w);


/* Create a mmap memory bucket */
APR_EXPORT(ap_bucket *) ap_bucket_mmap_create(const void *buf,
                                      ap_size_t nbytes, ap_ssize_t *w);

/* Create a read only memory bucket. */
APR_EXPORT(ap_bucket *) ap_bucket_rmem_create(const void *buf,
                               ap_size_t nbyte, ap_ssize_t *w);

/* Create an End of Stream bucket */
APR_EXPORT(ap_bucket *) ap_bucket_eos_create(void);

#endif

