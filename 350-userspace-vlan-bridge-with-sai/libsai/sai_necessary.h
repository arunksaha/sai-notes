#pragma once

// This is a minimalist subset of sai.h that is necessary for this project.

#include <saitypes.h>  // for sai_status_t
#include <saivlan.h>
#include <saifdb.h>
#include <saitypes.h>
#include <saistatus.h>
#include <saiswitch.h>

typedef enum _sai_api_t {
    SAI_API_UNSPECIFIED = 0,
    SAI_API_SWITCH      = 1,
    SAI_API_PORT        = 2,
    SAI_API_VLAN        = 3,
    SAI_API_FDB         = 4,
} sai_api_t;

