/**
 * collectd - src/utils_kstat.h
 * Copyright (C) 2015       Florian octo Forster
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *   Florian octo Forster <octo at collectd.org>
 **/

#ifndef UTILS_KSTAT_H
#define UTILS_KSTAT_H

#include "collectd.h"
#include "plugin.h"

#include <kstat.h>

/* ukstat_update updates the kstat chain and calls "callback" only if the chain
 * has changed. */
int ukstat_update (int (*callback) (void *), void *user_data);

/* ukstat_foreach iterates over all entries in the kstat chain and calls
 * "callback" for each. If "callback" returns non-zero, iterating is aborted
 * and the value is returned without calling "callback" again. */
int ukstat_foreach (int (*callback) (kstat_t *, void *), void *user_data);

/* ukstat_lookup is a wrapper around kstat_lookup. */
kstat_t *ukstat_lookup (char *ks_module, int ks_instance, char *ks_name);

/* ukstat_read is a wrapper around kstat_read. */
kid_t ukstat_read (kstat_t *ks, void *buf);

/* ukstat_gauge converts a named kstat entry to gauge. Returns non-zero on failure. */
int ukstat_gauge (kstat_t *ks, char *name, gauge_t *v);

/* ukstat_derive converts a named kstat entry to derive. Returns non-zero on failure. */
int ukstat_derive (kstat_t *ks, char *name, derive_t *v);

#endif
