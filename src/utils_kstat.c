/**
 * collectd - src/utils_kstat.c
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

#include "collectd.h"
#include "common.h"

#include "utils_kstat.h"

static kstat_ctl_t *kc = NULL;
static time_t       last_update = 0;
static kid_t        last_kcid = 0;

int ukstat_update (int (*callback) (void *), void *user_data) /* {{{ */
{
  kid_t kcid;

  if (kc == NULL)
  {
    kc = kstat_open ();
    if (kc == NULL)
    {
      ERROR ("utils_kstat: kstat_open failed");
      return -1;
    }
  }

  if ((time (NULL) - last_update) <= 1)
    return 0;

  kid_t kcid = kstat_chain_update (kc);
  if (kcid < 0)
  {
    char errbuf[1024];
    if (last_kcid == 0)
    {
      ERROR ("utils_kstat: kstat_chain_update failed: %s",
          sstrerror (errno, errbuf, sizeof (errbuf)));
      return -1;
    }
    WARNING ("utils_kstat: kstat_chain_update failed: %s",
        sstrerror (errno, errbuf, sizeof (errbuf)));
  }
  else if (kcid > 0)
  {
    last_kcid = kcid;
    DEBUG ("utils_kstat: successfully updated kstat chain to ID %d", (int) kcid);
  }
  last_update = time (NULL);

  if (callback != NULL)
    return (*callback) (user_data);
  else
    return 0;
} /* }}} int ukstat_update */

int ukstat_foreach (int (*callback) (kstat_t *, void *), void *user_data) /* {{{ */
{
  kstat_t *ks;

  for (ks = kc->kc_chain; ks != NULL; ks = ks->ks_next)
  {
    int status = (*callback) (ks, user_data);
    if (status != 0)
      return status;
  }

  return 0;
} /* }}} int ukstat_foreach */

kstat_t *ukstat_lookup (char *ks_module, int ks_instance, char *ks_name) /* {{{ */
{
  return kstat_lookup (kc, ks_module, ks_instance, ks_name);
} /* }}} ukstat_lookup */

kid_t ukstat_read (kstat_t *ks, void *buf) /* {{{ */
{
  return kstat_read (kc, ks, buf);
} /* }}} ukstat_read */

int ukstat_gauge (kstat_t *ks, char *name, gauge_t *v) /* {{{ */
{
  kstat_named_t *n;

  if ((ks == NULL) || (ks->ks_type != KSTAT_TYPE_NAMED) || (v == NULL))
    return EINVAL;

  n = kstat_data_lookup (ks, name);
  if (n == NULL)
    return ENOENT;

  switch (n->data_type)
  {
    case KSTAT_DATA_INT32:
      return (gauge_t) n->value.i32;
    case KSTAT_DATA_UINT32:
      return (gauge_t) n->value.ui32;
    case KSTAT_DATA_INT64:
      return (gauge_t) n->value.i64;
    case KSTAT_DATA_UINT64:
      return (gauge_t) n->value.ui64;
  }

  return EINVAL;
} /* }}} int ukstat_gauge */

int ukstat_derive (kstat_t *ks, char *name, derive_t *v) /* {{{ */
{
  kstat_named_t *n;

  if ((ks == NULL) || (ks->ks_type != KSTAT_TYPE_NAMED) || (v == NULL))
    return EINVAL;

  n = kstat_data_lookup (ks, name);
  if (n == NULL)
    return ENOENT;

  switch (n->data_type)
  {
    case KSTAT_DATA_INT32:
      return (derive_t) n->value.i32;
    case KSTAT_DATA_UINT32:
      return (derive_t) n->value.ui32;
    case KSTAT_DATA_INT64:
      return (derive_t) n->value.i64;
    case KSTAT_DATA_UINT64:
      return (derive_t) n->value.ui64;
  }

  return EINVAL;
} /* }}} int ukstat_derive */

/* vim: set sw=2 sts=2 et fdm=marker : */
