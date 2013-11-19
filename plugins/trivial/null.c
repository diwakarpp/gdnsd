/* Copyright © 2012 Brandon L Black <blblack@gmail.com>
 *
 * This file is part of gdnsd.
 *
 * gdnsd is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * gdnsd is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with gdnsd.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#define GDNSD_PLUGIN_NAME null

#include "config.h"
#include <gdnsd/plugin.h>
#include <string.h>

int plugin_null_map_res(const char* resname V_UNUSED, const uint8_t* origin V_UNUSED) {
    return 0;
}

bool plugin_null_resolve(unsigned threadnum V_UNUSED, unsigned resnum V_UNUSED, const uint8_t* origin, const client_info_t* cinfo V_UNUSED, dyn_result_t* result) {

    if(origin) {
        result->is_cname = true;
        gdnsd_dname_from_string(result->cname, (const uint8_t*)"invalid.", 8);
    }
    else {
        result->is_cname = false;
        result->a.count_v4 = 1;
        result->a.count_v6 = 1;
        result->a.addrs_v4[0] = 0;
        memset(result->a.addrs_v6, 0, 16);
    }

    return true;
}

// Obviously, we could implement "null" monitoring with simpler code,
//  but this exercises some API bits, so it's useful for testing

typedef struct {
    const char* name;
    unsigned interval;
} null_svc_t;

typedef struct {
    null_svc_t* svc;
    mon_smgr_t* smgr;
    ev_timer* interval_watcher;
} null_mon_t;

static unsigned num_svcs = 0;
static unsigned num_mons = 0;
static null_svc_t** null_svcs = NULL;
static null_mon_t** null_mons = NULL;

static void null_interval_cb(struct ev_loop* loop V_UNUSED, struct ev_timer* t, const int revents V_UNUSED) {
    dmn_assert(loop); dmn_assert(t);
    dmn_assert(revents == EV_TIMER);

    null_mon_t* mon = (null_mon_t*)t->data;
    dmn_assert(mon);
    gdnsd_mon_state_updater(mon->smgr, false);
}

void plugin_null_add_svctype(const char* name, const vscf_data_t* svc_cfg V_UNUSED, const unsigned interval, const unsigned timeout V_UNUSED) {
    dmn_assert(name);
    null_svcs = realloc(null_svcs, sizeof(null_svc_t*) * ++num_svcs);
    null_svc_t* this_svc = null_svcs[num_svcs - 1] = malloc(sizeof(null_svc_t));
    this_svc->name = strdup(name);
    this_svc->interval = interval;
}

void plugin_null_add_monitor(const char* svc_name, mon_smgr_t* smgr) {
    null_svc_t* this_svc = NULL;

    for(unsigned i = 0; i < num_svcs; i++) {
        if(!strcmp(svc_name, null_svcs[i]->name)) {
            this_svc = null_svcs[i];
            break;
        }
    }

    dmn_assert(this_svc);
    null_mons = realloc(null_mons, sizeof(null_mon_t*) * ++num_mons);
    null_mon_t* this_mon = null_mons[num_mons - 1] = malloc(sizeof(null_mon_t));
    this_mon->svc = this_svc;
    this_mon->smgr = smgr;
    this_mon->interval_watcher = malloc(sizeof(ev_timer));
    ev_timer_init(this_mon->interval_watcher, &null_interval_cb, 0, 0);
    this_mon->interval_watcher->data = this_mon;
}

void plugin_null_init_monitors(struct ev_loop* mon_loop) {
    dmn_assert(mon_loop);

    for(unsigned int i = 0; i < num_mons; i++) {
        ev_timer* ival_watcher = null_mons[i]->interval_watcher;
        ev_timer_set(ival_watcher, 0, 0);
        ev_timer_start(mon_loop, ival_watcher);
    }
}

void plugin_null_start_monitors(struct ev_loop* mon_loop) {
    dmn_assert(mon_loop);

    for(unsigned int i = 0; i < num_mons; i++) {
        null_mon_t* mon = null_mons[i];
        const unsigned ival = mon->svc->interval;
        const double stagger = (((double)i) / ((double)num_mons)) * ((double)ival);
        ev_timer* ival_watcher = mon->interval_watcher;
        ev_timer_set(ival_watcher, stagger, ival);
        ev_timer_start(mon_loop, ival_watcher);
    }
}

