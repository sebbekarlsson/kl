#ifndef KL_TEST_TESTS_H
#define KL_TEST_TESTS_H

// Umbrella header shared by every test translation unit. It pulls in the
// framework plus every library header a test suite might need, and declares
// all test functions so main.c can register them in the tests[] array.

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdalign.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <limits.h>

#include <test/framework.h>

#include <kl/macros.h>
#include <kl/string/string.h>
#include <kl/string/scanner.h>
#include <kl/memory/allocator.h>
#include <kl/templates/list/int64_list.h>
#include <kl/templates/list/blob_ptr_list.h>
#include <kl/templates/hashmap/int64_hashmap.h>
#include <kl/templates/sparse_set/binary_sparse_set.h>
#include <kl/binary/blob.h>
#include <kl/io/fs.h>
#include <kl/json/tokenizer.h>
#include <kl/json/parser.h>
#include <kl/path/path.h>
#include <kl/cli/args.h>

/* ===================== string (test_string.c) ===================== */
void test_str_const_slice();
void test_str_const_includes();
void test_str_scanner(KL_TestFunctionContext* ctx);

/* ===================== json (test_json.c) ===================== */
void test_json_tokenizer(KL_TestFunctionContext* ctx);
void test_json_parser(KL_TestFunctionContext* ctx);
void test_json_node_utils(KL_TestFunctionContext* ctx);

/* ===================== allocator (test_allocator.c) ===================== */
void test_allocator_init_destroy();
void test_allocator_double_init_is_noop();
void test_allocator_basic_malloc();
void test_allocator_zero_initialized_memory();
void test_allocator_multiple_mallocs_no_overlap();
void test_allocator_new_page_growth();
void test_allocator_large_alloc_grows_page();
void test_allocator_malloc_aligned();
void test_allocator_strdup();
void test_allocator_sprintf();
void test_allocator_reset_zeros_and_keeps_pages();
void test_allocator_reset_grows_capacity();
void test_allocator_realloc_grow_preserves_data();
void test_allocator_realloc_shrink_returns_same_pointer();
void test_allocator_realloc_from_zero_acts_as_malloc();
void test_allocator_tracked_malloc_records_allocations();
void test_allocator_free_zeros_memory_and_reuses_slot();
void test_allocator_save_restore_from_empty();
void test_allocator_save_restore_preserves_prior_data();
void test_allocator_save_restore_drops_new_pages();
void test_allocator_destroy_all_pages_keeps_allocator_usable();
void test_allocator_many_small_allocs();
void test_allocator_strdup_in_reused_page();
void test_allocator_tracked_realloc_grow_preserves_data();
void test_allocator_tracked_realloc_grow_marks_old_freed();
void test_allocator_tracked_realloc_grow_zeros_old_region();
void test_allocator_tracked_realloc_old_slot_reusable();
void test_allocator_tracked_realloc_new_region_is_freeable();
void test_allocator_tracked_realloc_shrink_preserves_pointer_and_tracking();
void test_allocator_tracked_realloc_from_zero_tracks_new_region();
void test_allocator_tracked_realloc_aligned();
void test_allocator_tracked_realloc_chain();
void test_allocator_tracked_realloc_reuses_pre_freed_slot();
void test_allocator_tracked_realloc_many_independent_handles();

/* ===================== int64_list (test_list.c) ===================== */
void test_int64_list_init_destroy();
void test_int64_list_double_init_is_noop();
void test_int64_list_push_basic();
void test_int64_list_push_many_preserves_order();
void test_int64_list_push_grows_capacity();
void test_int64_list_grow_increases_slots();
void test_int64_list_grow_zeros_new_slots();
void test_int64_list_pop_basic();
void test_int64_list_pop_lifo_order();
void test_int64_list_pop_with_null_out();
void test_int64_list_pop_empty_returns_zero();
void test_int64_list_pop_at_first();
void test_int64_list_pop_at_middle();
void test_int64_list_pop_at_last();
void test_int64_list_pop_at_empty_returns_zero();
void test_int64_list_push_start_basic();
void test_int64_list_push_start_reverses_insertion_order();
void test_int64_list_push_start_after_push();
void test_int64_list_push_start_grows_when_needed();
void test_int64_list_includes_found();
void test_int64_list_includes_not_found();
void test_int64_list_includes_empty();
void test_int64_list_includes_after_pop();
void test_int64_list_reset_clears_length();
void test_int64_list_reset_zeros_items();
void test_int64_list_reset_allows_reuse();
void test_int64_list_reset_on_empty_is_safe();
void test_int64_list_with_arena_allocator();
void test_int64_list_arena_grow_path();
void test_int64_list_push_pop_interleaved();
void test_int64_list_handles_negative_and_extreme_values();
void test_int64_list_stress_many_elements();
void test_int64_list_pop_at_drains_from_front();

/* ===================== int64_hashmap (test_hashmap.c) ===================== */
void test_int64_hashmap_init_destroy();
void test_int64_hashmap_double_init_is_noop();
void test_int64_hashmap_capacity_rounds_to_power_of_two();
void test_int64_hashmap_set_get_basic();
void test_int64_hashmap_get_missing_returns_zero();
void test_int64_hashmap_set_overwrite_updates_value();
void test_int64_hashmap_set_returns_valid_slot_index();
void test_int64_hashmap_has_before_and_after();
void test_int64_hashmap_unset_removes_entry();
void test_int64_hashmap_unset_missing_returns_zero();
void test_int64_hashmap_unset_then_reset_reinserts();
void test_int64_hashmap_get_ptr_allows_mutation();
void test_int64_hashmap_get_ptr_missing_returns_zero();
void test_int64_hashmap_hash_is_deterministic();
void test_int64_hashmap_collisions_with_tiny_capacity();
void test_int64_hashmap_many_entries_round_trip();
void test_int64_hashmap_unset_in_collision_chain();
void test_int64_hashmap_string_keys_fnv_path();
void test_int64_hashmap_keys_with_same_prefix_differ();
void test_int64_hashmap_get_keys_lists_all_set_keys();
void test_int64_hashmap_get_keys_excludes_unset();
void test_int64_hashmap_used_slots_tracking();
void test_int64_hashmap_get_used_slots_returns_set_only();
void test_int64_hashmap_reset_clears_entries();
void test_int64_hashmap_serialize_deserialize_round_trip();
void test_int64_hashmap_deserialize_rejects_bad_magic();
void test_int64_hashmap_zero_value_is_distinguishable();
void test_int64_hashmap_negative_and_extreme_keys();

/* ===================== binary_sparse_set (test_sparse_set.c) ===================== */
void test_binary_sparseset_init_destroy();
void test_binary_sparseset_double_init_is_noop();
void test_binary_sparseset_set_get_basic();
void test_binary_sparseset_get_missing_returns_null();
void test_binary_sparseset_set_null_data_zero_inits();
void test_binary_sparseset_set_overwrite_keeps_length();
void test_binary_sparseset_set_returns_pointer_into_values();
void test_binary_sparseset_has_before_and_after_set();
void test_binary_sparseset_has_out_of_range_returns_false();
void test_binary_sparseset_get_oob_returns_null();
void test_binary_sparseset_remove_missing_returns_false();
void test_binary_sparseset_remove_last_element_no_swap();
void test_binary_sparseset_remove_middle_swaps_with_last();
void test_binary_sparseset_remove_then_reinsert_same_id();
void test_binary_sparseset_remove_all_drains();
void test_binary_sparseset_reset_clears_entries();
void test_binary_sparseset_reset_preserves_capacity();
void test_binary_sparseset_reset_on_empty_is_safe();
void test_binary_sparseset_sparse_grows_for_large_id();
void test_binary_sparseset_dense_grows_when_full();
void test_binary_sparseset_at_iterates_dense_buffer();
void test_binary_sparseset_struct_payload_round_trip();
void test_binary_sparseset_many_inserts_round_trip();
void test_binary_sparseset_alignment_default();
void test_binary_sparseset_id_zero_works();

/* ===================== fs (test_fs.c) ===================== */
void test_fs_read_directory_rejects_bad_args();
void test_fs_read_directory_lists_project_src_non_recursive();
void test_fs_read_directory_recursive_descends_into_subdirs();

/* ===================== path (test_path.c) ===================== */
void test_path_is_absolute(KL_TestFunctionContext* ctx);
void test_path_dirname(KL_TestFunctionContext* ctx);
void test_path_basename(KL_TestFunctionContext* ctx);
void test_path_extname(KL_TestFunctionContext* ctx);
void test_path_join(KL_TestFunctionContext* ctx);
void test_path_normalize(KL_TestFunctionContext* ctx);
void test_path_resolve(KL_TestFunctionContext* ctx);
void test_path_cwd(KL_TestFunctionContext* ctx);

/* ===================== cli (test_cli.c) ===================== */
void test_cli_get_arg_space_syntax(KL_TestFunctionContext* ctx);
void test_cli_get_arg_equals_syntax(KL_TestFunctionContext* ctx);
void test_cli_get_arg_quoted_values(KL_TestFunctionContext* ctx);

#endif
