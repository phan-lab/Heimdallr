#ifndef _DEFS_H_
#define _DEFS_H_

#include <random>
#include <string.h>
#include <string>

#include "ns3/task-schedules.h"

/**
 * Modes
 */

#define REPUTATION_SYSTEM (1)
#define USE_TCP (0) // We currently also use TCP for LTE; for LAN we use UDP since it's reliable
#define USE_UDP (1 - USE_TCP)
/**
 *  parameters
 */
#define MILLI_TO_MICRO (1000)
#define DISTANCE_ENBS (10000) //!< in meters
#define NUM_ENBS (22)
#define FIND_CLOSEST_ENB(pos) ((int)(pos / DISTANCE_ENBS))

/* artifact-reproduction: made overridable from the Makefile.  Figure 10 needs
 * 400 s of simulated time and the track log, both of which were compiled out. */
#ifndef END_TIME
#define END_TIME (60) //!< In seconds
#endif

#define F_MAX (1)

#define PING_OFFSET_BY_ID(id) (13000 * id)
#define CONNECTION_SETUP_OFFSET (5000000) //!< In microseconds. Allow all TCP connections to setup
#define PING_INTERVAL (1000000)            //!< Heartbeat interval, in microseconds
#define MAX_SCHEDULE_VAR (50)
#define SCHEDULE_VAR (random() % MAX_SCHEDULE_VAR - (int)(MAX_SCHEDULE_VAR / 2))
#define INTRA_REGION_LATENCY (2) //!< in microseconds

#define INTRA_REGION_LATENCY_VAR (1000) //!< The variation. In microseconds

/* artifact-reproduction: D_t/o, the inter-region timeout.
 *
 * The paper's railway case study states "We set the heartbeat interval
 * R_hb = 1 s and timeout D_t/o = 200 ms" (Sec. VII-A), and Figure 10's
 * narrative - safe-mode transition at t = 10.2 s after a heartbeat due at
 * t = 10.0 s - only follows from a 200 ms timeout.  The tree ships 500000 us.
 * The railway Makefile overrides it to the paper's value; nothing else does,
 * so other users of this header keep the shipped behaviour. */
#ifndef PING_TIMEOUT
#define PING_TIMEOUT (500000)
#endif
// #define PING_TIMEOUT (950000)

#define NODE_PER_TRAIN (4)
#define NODE_PER_STATION (2)

#define STATION_RID (0) //!< The station is region 0, all others are trains
#define SENSOR_NID (0)
#define ACTUATOR_NID SENSOR_NID
#define MAX_TRAINS (100)   //!< Used to provide a prefix of task id.
#define MAX_SPEED (90)    //!< in m/s
#define ACTUATE_FREQ (100) //<! in milliseconds

#define MAX_NORMAL_ACC (0.6)   //!< in meters/second^2
#define MAX_EMERGENT_ACC (1.2) //!< in meters/second^2

/**
 *  Basic Macros
 */
#define ABS(x) ((x) > 0 ? (x) : (-(x)))
#define MAXBUF (65536) //!< The size of a buffer



/**
 * Measurements
*/
#ifndef ENABLE_TRACK_LOG
#define ENABLE_TRACK_LOG (0)
#endif

/**
 * artifact-reproduction: Figure 10 (Wenzhou incident) scenario selector.
 *
 * The shipped tree contains this scenario only as commented-out blocks in
 * train_app.cc, toggled by hand between runs.  FIG10_SCENARIO turns the three
 * variants into a compile-time choice so each can be produced from source.
 * 0 leaves the default behaviour of the tree untouched.
 */
#define FIG10_STABLE    (1)
#define FIG10_MANUAL    (2)
#define FIG10_HEIMDALLR (3)
#ifndef FIG10_SCENARIO
#define FIG10_SCENARIO  (0)
#endif
#if ENABLE_TRACK_LOG 
#include <string>
#define TRACK_LOG_FILE(rid) std::string("track_logs/train-") + std::to_string(rid) + std::string(".csv")
#endif

/**
 *  Types
 */
enum NodeType
{
    UNKNOWN,
    TRAIN,
    STATION
};

typedef struct MovementAuth
{
    double stop_by_location;
    double target_speed;
    int which_train;

    MovementAuth()
    {
        /**
         * Since c++ has auto padding of struct,
         * to keep valgrind quiet, init the memory.
         * Remove this for performance.
         */
        memset(this, 0, sizeof(*this));
    }
} MovementAuth;

#endif
