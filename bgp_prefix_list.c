#include <bgp/bgp.h>
#include <vlib/vlib.h>

bgp_prefix_list_t *bgp_find_or_create_prefix_list(bgp_main_t *bmp, const char *list_name) {
    bgp_prefix_list_t **list;

    vec_foreach(list, bmp->prefix_lists) {
        if (!strcmp((*list)->name, list_name)) {
            return *list; // Dereference to get the actual prefix list
        }
    }

    bgp_prefix_list_t *new_list = clib_mem_alloc(sizeof(bgp_prefix_list_t));
    memset(new_list, 0, sizeof(*new_list));
    strncpy(new_list->name, list_name, sizeof(new_list->name) - 1);
    vec_add1(bmp->prefix_lists, new_list); // Add the new list pointer to the vector

    clib_warning("Created new prefix list: %s", list_name);
    return new_list;
}

void bgp_update_prefix_list(bgp_main_t *bmp, const char *list_name, ip4_address_t *prefix, u8 mask_length, bool permit) {
    bgp_prefix_list_t *list = bgp_find_or_create_prefix_list(bmp, list_name);
    bgp_prefix_t *entry;

    entry = clib_mem_alloc(sizeof(bgp_prefix_t));
    entry->prefix = *prefix;
    entry->mask_length = mask_length;
    entry->permit = permit;

    vec_add1(list->entries, entry); // Add the new entry to the vector
    clib_warning("Added prefix %U/%u (%s) to list %s.",
                 format_ip4_address, prefix, mask_length,
                 permit ? "permit" : "deny", list_name);
}

void bgp_free_prefix_lists(bgp_main_t *bmp) {
    bgp_prefix_list_t **list;

    vec_foreach(list, bmp->prefix_lists) {
        vec_free((*list)->entries); // Free the entries vector
        clib_mem_free(*list);      // Free the prefix list itself
    }
    vec_free(bmp->prefix_lists);   // Free the prefix lists vector
}