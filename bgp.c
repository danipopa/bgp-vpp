#include <vnet/vnet.h>
#include <vnet/ip/ip.h>
#include <vnet/plugin/plugin.h>
#include <vlibmemory/api.h>
#include <bgp/bgp.h>
#include <vpp/app/version.h>


#define REPLY_MSG_ID_BASE bmp->msg_id_base
#include <vlibapi/api_helper_macros.h>

// Global BGP instance
bgp_main_t bgp_main;

// // === Utility Function Definitions ===
// int ip4_address_cmp(const ip4_address_t *a, const ip4_address_t *b) {
//     return memcmp(a, b, sizeof(ip4_address_t));
// }

// === Initialization Function ===
static clib_error_t *bgp_init(vlib_main_t *vm) {
    bgp_main_t *bmp = &bgp_main;

    bmp->vlib_main = vm;
    bmp->vnet_main = vnet_get_main();
    bmp->bgp_router_id = 0;        // Default router ID
    bmp->bgp_as_number = 0;        // Default AS number
    bmp->hold_time = 180;          // Default hold timer
    bmp->keepalive_time = 60;      // Default keepalive timer
    bmp->prefix_lists = NULL;      // Initialize prefix lists
    bmp->routes = NULL;            // Initialize routes pool
    bmp->aggregates = NULL;        // Initialize aggregates pool
    bmp->neighbors = NULL;         // Initialize neighbors pool

    clib_spinlock_init(&bmp->lock);

    clib_warning("BGP plugin initialized.");
    return 0;
}

VLIB_INIT_FUNCTION(bgp_init);

// === CLI Commands Registration ===
// #include <bgp/bgp_cli.c>

// === Plugin Registration ===
VLIB_PLUGIN_REGISTER() = {
    .version = VPP_BUILD_VER,
    .description = "BGP Plugin",
};

// === Cleanup Function ===
static clib_error_t *bgp_exit(vlib_main_t *vm) {
    bgp_main_t *bmp = &bgp_main;

    clib_spinlock_lock(&bmp->lock);

    // Free resources
    bgp_free_prefix_lists(bmp);     // Free prefix lists
    pool_free(bmp->routes);         // Free routes pool
    pool_free(bmp->aggregates);     // Free aggregates pool
    pool_free(bmp->neighbors);      // Free neighbors pool

    clib_spinlock_unlock(&bmp->lock);
    clib_spinlock_free(&bmp->lock);

    clib_warning("BGP plugin cleaned up.");
    return 0;
}

VLIB_MAIN_LOOP_EXIT_FUNCTION(bgp_exit);
