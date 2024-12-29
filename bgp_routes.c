#include <bgp/bgp.h>
#include <vlib/vlib.h>

// Add a new route
void bgp_add_route(bgp_main_t *bmp, ip4_address_t prefix, u8 mask_length, ip4_address_t next_hop) {
    bgp_route_t *route;

    clib_spinlock_lock(&bmp->lock);
    pool_get(bmp->routes, route);

    route->prefix = prefix;
    route->mask_length = mask_length;
    route->next_hop = next_hop;

    clib_warning("Added BGP route: %U/%d -> Next Hop: %U",
                 format_ip4_address, &prefix, mask_length,
                 format_ip4_address, &next_hop);

    clib_spinlock_unlock(&bmp->lock);
}

// Remove a route
void bgp_remove_route(bgp_main_t *bmp, ip4_address_t prefix, u8 mask_length) {
    bgp_route_t *route;

    clib_spinlock_lock(&bmp->lock);

    pool_foreach(route, bmp->routes) {
        if (!ip4_address_cmp(&route->prefix, &prefix) && route->mask_length == mask_length) {
            pool_put(bmp->routes, route);
            clib_warning("Removed BGP route: %U/%d", format_ip4_address, &prefix, mask_length);
            clib_spinlock_unlock(&bmp->lock);
            return;
        }
    }

    clib_warning("BGP route not found: %U/%d", format_ip4_address, &prefix, mask_length);
    clib_spinlock_unlock(&bmp->lock);
}

// Show all routes
void bgp_show_routes(bgp_main_t *bmp) {
    bgp_route_t *route;

    vlib_cli_output(bmp->vlib_main, "BGP Routes:");
    pool_foreach(route, bmp->routes) {
        vlib_cli_output(bmp->vlib_main, "Route: %U/%d -> Next Hop: %U",
                        format_ip4_address, &route->prefix, route->mask_length,
                        format_ip4_address, &route->next_hop);
    }
}

/**
 * Advertise a network prefix in BGP.
 */
int bgp_advertise_network(bgp_main_t *bmp, ip4_address_t prefix, u8 mask_length) {
    bgp_route_t *route;

    // Check if the route already exists
    pool_foreach(route, bmp->routes) {
        if (ip4_address_cmp(&route->prefix, &prefix) == 0 && route->mask_length == mask_length) {
            clib_warning("Network %U/%d is already advertised.", format_ip4_address, &prefix, mask_length);
            return -1; // Route already exists
        }
    };

    // Add the new route to the BGP routing table
    pool_get(bmp->routes, route);
    memset(route, 0, sizeof(bgp_route_t));
    route->prefix = prefix;
    route->mask_length = mask_length;
    route->next_hop.as_u32 = bmp->bgp_router_id; // Use the router ID as the next hop for advertised networks

    clib_warning("Advertised BGP network: %U/%d", format_ip4_address, &prefix, mask_length);

    return 0; // Success
}