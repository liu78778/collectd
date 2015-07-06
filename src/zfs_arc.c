/**
 * collectd - src/zfs_arc.c
 * Copyright (C) 2009  Anthony Dewhurst
 * Copyright (C) 2012  Aurelien Rougemont
 * Copyright (C) 2013  Xin Li
 * Copyright (C) 2014  Marc Fournier
 * Copyright (C) 2014  Wilfried Goesgens
 * Copyright (C) 2015  Florian octo Forster
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; only version 2 of the License is applicable.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 *
 * Authors:
 *   Anthony Dewhurst <dewhurst at gmail>
 *   Aurelien Rougemont <beorn at gandi.net>
 *   Xin Li <delphij at FreeBSD.org>
 *   Marc Fournier <marc.fournier at camptocamp.com>
 *   Wilfried Goesgens <dothebart at citadel.org>
 *   Florian octo Forster <octo at collectd.org>
 **/

#include "collectd.h"
#include "common.h"
#include "plugin.h"

#if KERNEL_LINUX
# include "utils_llist.h"
# define ZFS_ARCSTATS_FILE "/proc/spl/kstat/zfs/arcstats"
/* #endif KERNEL_LINUX */

#elif KERNEL_SOLARIS
/* #endif KERNEL_SOLARIS */

#elif defined(__FreeBSD__)
# include <sys/types.h>
# include <sys/sysctl.h>
# define ZFS_ARCSTATS_PREFIX "kstat.zfs.misc.arcstats."
#endif /* __FreeBSD__ */

/*
 * Global variables
 */
#if KERNEL_LINUX
static llist_t *metrics;
/* #endif KERNEL_LINUX */

#elif KERNEL_SOLARIS
static kstat_t *ks;
/* #endif KERNEL_SOLARIS */

#elif defined(__FreeBSD__)
#endif /* __FreeBSD__ */

/*
 * Functions
 *
 * There are three operating system dependent functions:
 *   zfs_update     - read the latest counters from the OS
 *   zfs_get_derive - return a specific gauge value
 *   zfs_get_gauge  - return a specific derive value
 */
#if KERNEL_LINUX
static int zfs_put_value (char const *name, char const *value)
{
	llentry_t *e;

	e = llist_search (metrics, name);
	if (e != NULL)
	{
		sfree (e->value);
		e->value = strdup (value);
		return 0;
	}

	e = malloc (sizeof (*e));
	if (e == NULL)
		return ENOMEM;
	memset (e, 0, sizeof (*e));

	e->key = strdup (name);
	e->value = strdup (value);
	llist_append (metrics, e);
	return 0;
}

static int zfs_update (void)
{
	llentry_t *e;
	FILE *fh;
	char buffer[1024];

	for (e = llist_head (metrics); e != NULL;)
	{
		llentry_t *next = e->next;

		sfree (e->key);
		sfree (e->value);
		sfree (e);

		e = next;
	}

	if ((fh = fopen (ZFS_ARCSTATS_FILE, "r")) == NULL)
	{
		char errbuf[1024];
		WARNING ("zfs_arc plugin: fopen (\"%s\") failed: %s", ZFS_ARCSTATS_FILE,
				sstrerror (errno, errbuf, sizeof (errbuf)));
		return (-1);
	}

	while (fgets (buffer, sizeof (buffer), fh) != NULL)
	{
		char *fields[8];
		int numfields;

		numfields = strsplit (buffer, fields, STATIC_ARRAY_SIZE (fields));
		if (numfields != 3)
			continue;

		zfs_put_value (fields[0], fields[2]);
	} /* while (fgets) */

	fclose (fh);
	return 0;
} /* int zfs_update */

static int zfs_get_gauge (char const *name, gauge_t *g)
{
	llentry_t *e;
	value_t v;
	int status;

	e = llist_search (metrics, name);
	if (e == NULL)
		return ENOENT;

	status = parse_value (e->value, &v, DS_TYPE_GAUGE);
	if (status != 0)
		return status;

	*g = v.gauge;
	return 0;
}

static int zfs_get_derive (char const *name, derive_t *d)
{
	llentry_t *e;
	value_t v;
	int status;

	e = llist_search (metrics, name);
	if (e == NULL)
		return ENOENT;

	status = parse_value (e->value, &v, DS_TYPE_DERIVE);
	if (status != 0)
		return status;

	*d = v.derive;
	return 0;
}
/* #endif KERNEL_LINUX */

#elif KERNEL_SOLARIS
static int zfs_update (void)
{
	int status;

	status = ukstat_update (NULL, NULL);
	if (status != 0)
		return status;

	ks = ukstat_lookup ("zfs", 0, "arcstats");
	if (ks == NULL)
	{
		ERROR ("zfs_arc plugin: Cannot find zfs:0:arcstats kstat.");
		return ENOENT;
	}

	return 0;
}

static int zfs_get_gauge (char const *name, gauge_t *v)
{
	char tmp[DATA_MAX_NAME_LEN];

	sstrncpy (tmp, name, sizeof (tmp));
	return ukstat_gauge (ks, tmp, &v);
}

static int zfs_get_derive (char const *name, derive_t *v)
{
	char tmp[DATA_MAX_NAME_LEN];

	sstrncpy (tmp, name, sizeof (tmp));
	return ukstat_derive (ks, tmp, &v);
}
/* #endif KERNEL_SOLARIS */

#elif defined(__FreeBSD__)
static int zfs_update (void)
{
	return 0;
}

static int zfs_get_derive (char const *name, derive_t *v)
{
	char buffer[256];
	long long value = 0;
	size_t value_size = sizeof (value);
	int status;

	ssnprintf (buffer, sizeof (buffer), "%s%s", ZFS_ARCSTATS_PREFIX, name);
	status = sysctlbyname (buffer, &value, &value_size,
			/* new value = */ NULL, /* new length = */ 0);
	if (status != 0)
		return status;

	*v = (derive_t) value;
	return 0;
}

static int zfs_get_gauge (char const *name, gauge_t *v)
{
	derive_t d;
	int status;

	status = zfs_get_derive (name, &d);
	if (status != 0)
		return status;

	*v = (gauge_t) d;
	return 0;
}
#endif /* __FreeBSD__ */

static void za_submit (const char* type, const char* type_instance, value_t* values, size_t values_len)
{
	value_list_t vl = VALUE_LIST_INIT;

	vl.values = values;
	vl.values_len = values_len;

	sstrncpy (vl.host, hostname_g, sizeof (vl.host));
	sstrncpy (vl.plugin, "zfs_arc", sizeof (vl.plugin));
	sstrncpy (vl.type, type, sizeof (vl.type));
	sstrncpy (vl.type_instance, type_instance, sizeof (vl.type_instance));

	plugin_dispatch_values (&vl);
}

/* za_read_derive retrieves and submits a derive value. */
static int za_read_derive (char const *name, char const *type, char const *type_instance)
{
	value_t v;
	int status;

	status = zfs_get_derive (name, &v.derive);
	if (status != 0)
		return status;

	za_submit (type, type_instance, /* values = */ &v, /* values_num = */ 1);
	return 0;
} /* }}} int za_read_value */

/* za_read_gauge retrieves and submits a gauge value. */
static int za_read_gauge (char const *name, char const *type, char const *type_instance)
{
	value_t v;
	int status;

	status = zfs_get_gauge (name, &v.gauge);
	if (status != 0)
		return status;

	za_submit (type, type_instance, /* values = */ &v, /* values_num = */ 1);
	return 0;
} /* }}} int za_read_gauge */

static void za_submit_ratio (const char* type_instance, gauge_t hits, gauge_t misses)
{
	value_t ratio = {.gauge = NAN};

	if (!isfinite (hits) || (hits < 0.0))
		hits = 0.0;
	if (!isfinite (misses) || (misses < 0.0))
		misses = 0.0;

	if ((hits != 0.0) || (misses != 0.0))
		ratio.gauge = hits / (hits + misses);

	za_submit ("cache_ratio", type_instance, &ratio, /* value_num = */ 1);
}

static int za_read (void)
{
	gauge_t  arc_hits, arc_misses, l2_hits, l2_misses;
	value_t  l2_io[2];
	int status;

	status = zfs_update ();
	if (status != 0)
		return status;

	/* Sizes */
	za_read_gauge ("size",    "cache_size", "arc");

	/* The "l2_size" value has disappeared from Solaris some time in early
	 * 2013, and has only reappeared recently in Solaris 11.2. Stop trying
	 * if we ever fail to read it, so we don't spam the log. */
	static _Bool l2_size_avail = 1;
	if (l2_size_avail)
	{
		if (za_read_gauge ("l2_size", "cache_size", "L2") != 0)
			l2_size_avail = 0;
	}

	/* Operations */
	za_read_derive ("deleted",  "cache_operation", "deleted");
#if __FreeBSD__
	za_read_derive ("allocated","cache_operation", "allocated");
	za_read_derive ("stolen",   "cache_operation", "stolen");
#endif

	/* Issue indicators */
	za_read_derive ("mutex_miss", "mutex_operations", "miss");
	za_read_derive ("hash_collisions", "hash_collisions", "");

	/* Evictions */
	za_read_derive ("evict_l2_cached",     "cache_eviction", "cached");
	za_read_derive ("evict_l2_eligible",   "cache_eviction", "eligible");
	za_read_derive ("evict_l2_ineligible", "cache_eviction", "ineligible");

	/* Hits / misses */
	za_read_derive ("demand_data_hits",         "cache_result", "demand_data-hit");
	za_read_derive ("demand_metadata_hits",     "cache_result", "demand_metadata-hit");
	za_read_derive ("prefetch_data_hits",       "cache_result", "prefetch_data-hit");
	za_read_derive ("prefetch_metadata_hits",   "cache_result", "prefetch_metadata-hit");
	za_read_derive ("demand_data_misses",       "cache_result", "demand_data-miss");
	za_read_derive ("demand_metadata_misses",   "cache_result", "demand_metadata-miss");
	za_read_derive ("prefetch_data_misses",     "cache_result", "prefetch_data-miss");
	za_read_derive ("prefetch_metadata_misses", "cache_result", "prefetch_metadata-miss");

	/* Ratios */
	zfs_get_gauge ("hits",      &arc_hits);
	zfs_get_gauge ("misses",    &arc_misses);
	za_submit_ratio ("arc", arc_hits, arc_misses);

	zfs_get_gauge ("l2_hits",   &l2_hits);
	zfs_get_gauge ("l2_misses", &l2_misses);
	za_submit_ratio ("L2", l2_hits, l2_misses);

	/* I/O */
	zfs_get_derive ("l2_read_bytes",  &l2_io[0].derive);
	zfs_get_derive ("l2_write_bytes", &l2_io[1].derive);
	za_submit ("io_octets", "L2", l2_io, /* num values = */ 2);

	return (0);
} /* int za_read */

void module_register (void)
{
	plugin_register_read ("zfs_arc", za_read);
} /* void module_register */

/* vmi: set sw=8 noexpandtab fdm=marker : */
