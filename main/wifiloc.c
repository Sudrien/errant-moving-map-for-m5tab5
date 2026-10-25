/*
 * wifiloc.c -- see wifiloc.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "wifiloc.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* Not in C11's <math.h>. */
#ifndef M_PI
#define M_PI (3.14159265358979323846)
#endif

const char WIFILOC_HEADER[] =
    "key,obs,lat,lon,min_lat,max_lat,min_lon,max_lon,best_rssi,mobile";
/* The original's, keyed by the whole BSSID: still read, and merged. */
static const char WIFILOC_HEADER_BSSID[] =
    "bssid,obs,lat,lon,min_lat,max_lat,min_lon,max_lon,best_rssi,mobile";

void wifiloc_init(wifiloc_db_t *db, wifiloc_rec_t *tab, uint32_t cap)
{
    db->tab = tab;
    db->cap = cap;
    db->count = 0;
    db->dirty = 0;
    if (tab) memset(tab, 0, (size_t)cap * sizeof(*tab));
}

/* Bytes 2-5 of the six, big-endian: the router, not the network. A
 * router's guest, mesh and second-band networks differ in the last byte
 * (b4:20:46:40:06:e4 / e8) or in the first (96:76:3f:4d:af:df /
 * 9a:76:3f:4d:af:df), and are one place. Departs from the original, which
 * kept all six (0038). */
static uint64_t key(const uint8_t *b)
{
    uint64_t v = 0;
    for (int i = 1; i < 5; i++) v = (v << 8) | b[i];
    return v;
}

/* Whether aps[i] stands for its router in this scan: heard well enough,
 * and no other network of the same router heard stronger (the earlier
 * of two equal). One router counts once a scan, at its best. */
static bool rep(const wifiloc_ap_t *aps, int n, int i)
{
    if (aps[i].rssi < WIFILOC_MIN_RSSI) return false;
    const uint64_t k = key(aps[i].bssid);
    for (int j = 0; j < n; j++) {
        if (j == i || aps[j].rssi < WIFILOC_MIN_RSSI || key(aps[j].bssid) != k) continue;
        if (aps[j].rssi > aps[i].rssi || (aps[j].rssi == aps[i].rssi && j < i)) return false;
    }
    return true;
}

/* Of the routers in a scan, aps[i]'s place by strength: 0 the strongest,
 * the earlier of two equal first. */
static int rank(const wifiloc_ap_t *aps, int n, int i)
{
    int r = 0;
    for (int j = 0; j < n; j++) {
        if (j == i || !rep(aps, n, j)) continue;
        if (aps[j].rssi > aps[i].rssi || (aps[j].rssi == aps[i].rssi && j < i)) r++;
    }
    return r;
}

/* src: original/wifiloc.cpp lat_m(), lon_m(): 111320 m a degree, for
 * threshold comparisons, not a projection. */
static double lat_m(double dlat) { return dlat * 111320.0; }
static double lon_m(double dlon, double at_lat) { return dlon * 111320.0 * cos(at_lat * M_PI / 180.0); }

/* Judge a record mobile, for good, if heard over more than
 * WIFILOC_MOBILE_M after WIFILOC_MIN_OBS; true if it is newly so. */
static bool judge(wifiloc_rec_t *r, double at_lat)
{
    if (r->mobile || r->n < WIFILOC_MIN_OBS) return false;
    const double dx = lon_m((double)r->max_lon - (double)r->min_lon, at_lat);
    const double dy = lat_m((double)r->max_lat - (double)r->min_lat);
    if (sqrt(dx * dx + dy * dy) <= WIFILOC_MOBILE_M) return false;
    r->mobile = 1;
    return true;
}

static wifiloc_rec_t *find(const wifiloc_db_t *db, uint64_t id)
{
    for (uint32_t i = 0; i < db->count; i++)
        if (db->tab[i].id == id) return &db->tab[i];
    return NULL;
}

void wifiloc_learn(wifiloc_db_t *db, const wifiloc_ap_t *aps, int n,
                   double lat, double lon, int *added, int *folded, int *mobile)
{
    int a = 0, f = 0, m = 0;
    for (int i = 0; i < n; i++) {
        if (!rep(aps, n, i) || rank(aps, n, i) >= WIFILOC_LEARN_APS) continue;
        const uint64_t id = key(aps[i].bssid);
        wifiloc_rec_t *r = find(db, id);
        if (!r) {
            if (db->count >= db->cap) continue;     /* full: keep what is known */
            r = &db->tab[db->count++];
            memset(r, 0, sizeof(*r));
            r->id = id;
            r->min_lat = r->max_lat = (float)lat;
            r->min_lon = r->max_lon = (float)lon;
            r->best_rssi = -127;
            a++;
        } else {
            f++;
        }
        r->n++;
        r->sum_lat += lat;
        r->sum_lon += lon;
        if ((float)lat < r->min_lat) r->min_lat = (float)lat;
        if ((float)lat > r->max_lat) r->max_lat = (float)lat;
        if ((float)lon < r->min_lon) r->min_lon = (float)lon;
        if ((float)lon > r->max_lon) r->max_lon = (float)lon;
        if (aps[i].rssi > r->best_rssi) r->best_rssi = aps[i].rssi;
        if (judge(r, lat)) m++;
        db->dirty++;
    }
    if (added) *added = a;
    if (folded) *folded = f;
    if (mobile) *mobile = m;
}

bool wifiloc_locate(const wifiloc_db_t *db, const wifiloc_ap_t *aps, int n,
                    wifiloc_est_t *est)
{
    struct { double lat, lon, w; } hit[WIFILOC_HITS];
    int hits = 0;
    double wsum = 0, wlat = 0, wlon = 0;
    for (int i = 0; i < n && hits < WIFILOC_HITS; i++) {
        if (!rep(aps, n, i)) continue;
        const wifiloc_rec_t *r = find(db, key(aps[i].bssid));
        if (!r || r->mobile || r->n < WIFILOC_MIN_OBS) continue;
        /* Received power, linear: 10 dB stronger counts ten times. The
         * whole of the weighting model. */
        const double w = pow(10.0, aps[i].rssi / 10.0);
        hit[hits].lat = r->sum_lat / r->n;
        hit[hits].lon = r->sum_lon / r->n;
        hit[hits].w = w;
        wsum += w;
        wlat += w * hit[hits].lat;
        wlon += w * hit[hits].lon;
        hits++;
    }
    est->used = hits;
    if (hits < WIFILOC_MIN_APS || wsum <= 0) return false;
    est->lat = wlat / wsum;
    est->lon = wlon / wsum;
    /* The spread of the centroids about the mean, weighted the same:
     * honest in a way a metres-per-dBm figure would not be. */
    double vs = 0;
    for (int i = 0; i < hits; i++) {
        const double dx = lon_m(hit[i].lon - est->lon, est->lat);
        const double dy = lat_m(hit[i].lat - est->lat);
        vs += hit[i].w * (dx * dx + dy * dy);
    }
    est->acc_m = (float)sqrt(vs / wsum);
    return true;
}

/* ---- the file ---- */

bool wifiloc_header_ok(const char *line)
{
    return line && (strncmp(line, WIFILOC_HEADER, strlen(WIFILOC_HEADER)) == 0 ||
                    strncmp(line, WIFILOC_HEADER_BSSID, strlen(WIFILOC_HEADER_BSSID)) == 0);
}

bool wifiloc_parse(wifiloc_db_t *db, const char *line)
{
    unsigned b[6] = { 0 };
    unsigned long obs;
    double lat, lon, mnla, mxla, mnlo, mxlo;
    int rssi, mob;
    /* All the fields, or it is not a record: a key of four bytes, or the
     * original's whole BSSID of six, whose router is bytes 2-5. */
    if (sscanf(line, "%2x:%2x:%2x:%2x,%lu,%lf,%lf,%lf,%lf,%lf,%lf,%d,%d",
               &b[1], &b[2], &b[3], &b[4], &obs,
               &lat, &lon, &mnla, &mxla, &mnlo, &mxlo, &rssi, &mob) != 13 &&
        sscanf(line, "%2x:%2x:%2x:%2x:%2x:%2x,%lu,%lf,%lf,%lf,%lf,%lf,%lf,%d,%d",
               &b[0], &b[1], &b[2], &b[3], &b[4], &b[5], &obs,
               &lat, &lon, &mnla, &mxla, &mnlo, &mxlo, &rssi, &mob) != 15)
        return false;
    if (!obs || obs > UINT32_MAX || !isfinite(lat) || !isfinite(lon) ||
        fabs(lat) > 90.0 || fabs(lon) > 180.0)
        return false;
    uint8_t addr[6];
    for (int i = 0; i < 6; i++) addr[i] = (uint8_t)b[i];
    const uint64_t id = key(addr);
    const int8_t best = (int8_t)(rssi < -128 ? -128 : rssi > 127 ? 127 : rssi);

    /* A second row for the same router -- the original's file, a network
     * each -- is merged: the sums added, the extents joined, the best
     * kept, mobile if either was or the two together are. */
    wifiloc_rec_t *r = find(db, id);
    if (r) {
        if (r->n > UINT32_MAX - obs) return false;
        r->n += (uint32_t)obs;
        r->sum_lat += lat * (double)obs;
        r->sum_lon += lon * (double)obs;
        if ((float)mnla < r->min_lat) r->min_lat = (float)mnla;
        if ((float)mxla > r->max_lat) r->max_lat = (float)mxla;
        if ((float)mnlo < r->min_lon) r->min_lon = (float)mnlo;
        if ((float)mxlo > r->max_lon) r->max_lon = (float)mxlo;
        if (best > r->best_rssi) r->best_rssi = best;
        if (mob) r->mobile = 1;
        judge(r, r->sum_lat / r->n);
        return true;
    }
    if (db->count >= db->cap) return false;
    r = &db->tab[db->count++];
    memset(r, 0, sizeof(*r));
    r->id = id;
    r->n = (uint32_t)obs;
    /* The file has the mean; the arithmetic wants the sum. */
    r->sum_lat = lat * (double)obs;
    r->sum_lon = lon * (double)obs;
    r->min_lat = (float)mnla;
    r->max_lat = (float)mxla;
    r->min_lon = (float)mnlo;
    r->max_lon = (float)mxlo;
    r->best_rssi = best;
    r->mobile = mob ? 1 : 0;
    return true;
}

int wifiloc_format(const wifiloc_db_t *db, uint32_t i, char *out, size_t cap)
{
    if (i >= db->count || !db->tab[i].n) return -1;
    const wifiloc_rec_t *r = &db->tab[i];
    uint8_t b[4];
    uint64_t v = r->id;
    for (int k = 3; k >= 0; k--) { b[k] = (uint8_t)(v & 0xFF); v >>= 8; }
    /* Seven places, about a centimetre: so a round trip changes nothing a
     * later fold would notice, not because it is known that well. */
    const int k = snprintf(out, cap, "%02x:%02x:%02x:%02x,%lu,"
                                     "%.7f,%.7f,%.7f,%.7f,%.7f,%.7f,%d,%d",
                           b[0], b[1], b[2], b[3], (unsigned long)r->n,
                           r->sum_lat / r->n, r->sum_lon / r->n,
                           (double)r->min_lat, (double)r->max_lat,
                           (double)r->min_lon, (double)r->max_lon,
                           (int)r->best_rssi, (int)r->mobile);
    return (k < 0 || (size_t)k >= cap) ? -1 : k;
}

bool wifiloc_write_due(const wifiloc_db_t *db, uint32_t now_ms, uint32_t last_ms, bool written)
{
    if (!db->dirty) return false;
    if (db->dirty >= WIFILOC_WRITE_DIRTY) return true;
    return !written || now_ms - last_ms >= WIFILOC_WRITE_MS;
}

/* ---- when to scan ---- */

static double dist_m(double lat0, double lon0, double lat1, double lon1)
{
    const double dx = lon_m(lon1 - lon0, lat1), dy = lat_m(lat1 - lat0);
    return sqrt(dx * dx + dy * dy);
}

wifiloc_why_t wifiloc_next(wifiloc_plan_t *p, uint32_t now_ms, bool fine, bool coarse,
                           double kmh, double lat, double lon, bool have_records)
{
    if (fine) {
        p->lost = false;
        /* Only a fine fix teaches: a smeared centroid is believed later,
         * when there is nothing to check it against. */
        if (p->learned && now_ms - p->last_learn < WIFILOC_LEARN_MS) return WIFILOC_IDLE;
        if (kmh > WIFILOC_LEARN_MAX_KMH) return WIFILOC_IDLE;
        if (p->learned && dist_m(p->learn_lat, p->learn_lon, lat, lon) < WIFILOC_LEARN_M)
            return WIFILOC_IDLE;
        p->learned = true;
        p->last_learn = now_ms;
        p->learn_lat = lat;
        p->learn_lon = lon;
        return WIFILOC_LEARN;
    }
    /* A coarse fix is a position: nothing to locate, nothing to learn. */
    if (coarse) {
        p->lost = false;
        return WIFILOC_IDLE;
    }
    if (!p->lost) {
        p->lost = true;
        p->lost_since = now_ms;
        return WIFILOC_IDLE;
    }
    if (now_ms - p->lost_since < WIFILOC_LOST_MS) return WIFILOC_IDLE;
    if (p->located && now_ms - p->last_locate < WIFILOC_RETRY_MS) return WIFILOC_IDLE;
    if (!have_records) return WIFILOC_IDLE;
    p->located = true;
    p->last_locate = now_ms;
    return WIFILOC_LOCATE;
}

bool wifiloc_keep(double lat0, double lon0, uint32_t t0_ms, uint32_t now_ms,
                  bool fine, double lat, double lon)
{
    if (!fine) return false;
    const double allow = WIFILOC_LEARN_MAX_KMH / 3.6 * (double)(now_ms - t0_ms) / 1000.0 + 10.0;
    return dist_m(lat0, lon0, lat, lon) <= allow;
}
