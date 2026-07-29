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

#include <kl/test/framework.h>

#include <kl/macros.h>
#include <kl/string/string.h>
#include <kl/string/scanner.h>
#include <kl/string/jaro_winkler.h>
#include <kl/memory/allocator.h>
#include <kl/templates/list/int64_list.h>
#include <kl/templates/list/blob_ptr_list.h>
#include <kl/templates/unique_list/int64_unique_list.h>
#include <kl/templates/hashmap/int64_hashmap.h>
#include <kl/templates/sparse_set/binary_sparse_set.h>
#include <kl/templates/sparse_set/dummy.h>
#include <kl/templates/ringbuffer/f32_ringbuffer.h>
#include <kl/binary/blob.h>
#include <kl/binary/serial.h>
#include <kl/io/fs.h>
#include <kl/json/tokenizer.h>
#include <kl/json/parser.h>
#include <kl/path/path.h>
#include <kl/cli/args.h>
#include <kl/cli/program.h>
#include <kl/math/vec2.h>
#include <kl/math/vec3.h>
#include <kl/math/vec4.h>
#include <kl/math/vecn.h>
#include <kl/math/matn.h>
#include <kl/math/macros.h>
#include <kl/common/number.h>
#include <kl/noise/noise.h>

/* ===================== string (test_string.c) ===================== */
void test_str_const_eq();
void test_str_const_slice();
void test_str_const_includes();
void test_str_const_remove_numeric_prefix(KL_TestFunctionContext* ctx);
void test_str_const_remove_numeric_suffix(KL_TestFunctionContext* ctx);
void test_str_const_replace(KL_TestFunctionContext* ctx);
void test_str_buffer_append_fmt(KL_TestFunctionContext* ctx);
void test_str_scanner(KL_TestFunctionContext* ctx);
void test_jaro_winkler_identical_returns_one();
void test_jaro_winkler_empty_returns_zero();
void test_jaro_winkler_disjoint_returns_zero();
void test_jaro_winkler_martha_marhta();
void test_jaro_winkler_dixon_dicksonx();
void test_jaro_winkler_is_symmetric();
void test_jaro_winkler_prefix_capped_at_four();
void test_jaro_winkler_stays_bounded_for_long_shared_prefix();
void test_jaro_winkler_single_char();

/* ===================== json (test_json.c) ===================== */
void test_json_tokenizer(KL_TestFunctionContext* ctx);
void test_json_parser(KL_TestFunctionContext* ctx);
void test_json_node_utils(KL_TestFunctionContext* ctx);
void test_json_string_escapes(KL_TestFunctionContext* ctx);

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
void test_allocator_free_reuses_slot_aligned();
void test_allocator_reuses_freed_slot_before_bump();
void test_allocator_reused_handle_can_be_freed();
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
void test_int64_list_arena_destroy_frees_allocation();
void test_int64_list_arena_grow_path();
void test_int64_list_push_pop_interleaved();
void test_int64_list_handles_negative_and_extreme_values();
void test_int64_list_stress_many_elements();
void test_int64_list_pop_at_drains_from_front();
void test_int64_list_concat_basic();
void test_int64_list_concat_onto_empty();
void test_int64_list_concat_empty_source_is_noop();
void test_int64_list_concat_large_forces_grow();

/* ===================== int64_unique_list (test_unique_list.c) ===================== */
void test_int64_unique_list_init_destroy();
void test_int64_unique_list_double_init_is_noop();
void test_int64_unique_list_push_basic();
void test_int64_unique_list_push_many_preserves_order();
void test_int64_unique_list_push_duplicate_rejected();
void test_int64_unique_list_push_duplicates_interleaved();
void test_int64_unique_list_includes();
void test_int64_unique_list_includes_empty();
void test_int64_unique_list_index_of();
void test_int64_unique_list_index_of_missing_returns_negative();
void test_int64_unique_list_pop_basic();
void test_int64_unique_list_pop_lifo_order();
void test_int64_unique_list_pop_empty_returns_false();
void test_int64_unique_list_pop_then_repush();
void test_int64_unique_list_pop_at_middle_reindexes();
void test_int64_unique_list_pop_at_first_reindexes();
void test_int64_unique_list_pop_at_last_keeps_others();
void test_int64_unique_list_pop_at_out_of_range_returns_false();
void test_int64_unique_list_pop_at_empty_returns_false();
void test_int64_unique_list_pop_at_drains_from_front();
void test_int64_unique_list_remove_by_value();
void test_int64_unique_list_remove_missing_returns_false();
void test_int64_unique_list_grow_increases_slots();
void test_int64_unique_list_reset_clears_membership();
void test_int64_unique_list_reset_allows_reuse();
void test_int64_unique_list_handles_negative_and_extreme_values();
void test_int64_unique_list_with_arena_allocator();
void test_int64_unique_list_stress_dedup();

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
void test_int64_hashmap_custom_hash_is_invoked();
void test_int64_hashmap_custom_hash_constant_forces_collisions();
void test_int64_hashmap_custom_hash_overrides_builtin();

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
void test_binary_sparseset_noalloc_init_destroy();
void test_binary_sparseset_noalloc_set_get_basic();
void test_binary_sparseset_noalloc_sparse_grows_for_large_id();
void test_binary_sparseset_noalloc_dense_grows_when_full();
void test_binary_sparseset_noalloc_many_inserts_round_trip();
void test_binary_sparseset_noalloc_reset_then_reuse();

/* ===================== sparse_set (dummy, no allocator) (test_sparse_set.c) ===================== */
void test_sparse_set_noalloc_init_destroy();
void test_sparse_set_noalloc_set_get_basic();
void test_sparse_set_noalloc_set_overwrite_keeps_length();
void test_sparse_set_noalloc_has_and_remove();
void test_sparse_set_noalloc_id_zero_and_value_zero();
void test_sparse_set_noalloc_sparse_grows_for_large_id();
void test_sparse_set_noalloc_dense_grows_when_full();
void test_sparse_set_noalloc_remove_swaps_with_last();
void test_sparse_set_noalloc_many_inserts_round_trip();
void test_sparse_set_noalloc_reset_then_reuse();

/* ===================== f32_ringbuffer (test_ringbuffer.c) ===================== */
void test_f32_ringbuffer_init_destroy();
void test_f32_ringbuffer_double_init_is_noop();
void test_f32_ringbuffer_write_read_round_trip();
void test_f32_ringbuffer_write_advances_write_cursor();
void test_f32_ringbuffer_read_advances_read_cursor();
void test_f32_ringbuffer_write_cursor_wraps();
void test_f32_ringbuffer_read_cursor_wraps();
void test_f32_ringbuffer_full_cycle_returns_to_origin();
void test_f32_ringbuffer_overwrite_keeps_latest_in_slots();
void test_f32_ringbuffer_interleaved_read_write_is_fifo();
void test_f32_ringbuffer_reset_zeros_and_rewinds();
void test_f32_ringbuffer_reset_allows_reuse();
void test_f32_ringbuffer_length_one();
void test_f32_ringbuffer_preserves_special_values();
void test_f32_ringbuffer_stress_many_laps();
void test_f32_ringbuffer_with_arena_allocator();

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
void test_cli_program_positionals(KL_TestFunctionContext* ctx);
void test_cli_program_positional_defaults_and_required(KL_TestFunctionContext* ctx);
void test_cli_program_typed_and_extra_positionals(KL_TestFunctionContext* ctx);

/* ===================== binary_pool (test_binary_pool.c) ===================== */
void test_binary_pool_init_destroy();
void test_binary_pool_double_init_is_noop();
void test_binary_pool_acquire_basic();
void test_binary_pool_acquire_returns_distinct_slots();
void test_binary_pool_acquire_respects_element_stride();
void test_binary_pool_acquire_pads_size_to_alignment();
void test_binary_pool_write_round_trip();
void test_binary_pool_struct_payload_round_trip();
void test_binary_pool_acquire_exhausted_returns_zero();
void test_binary_pool_get_slot_before_acquire_returns_zero();
void test_binary_pool_get_slot_matches_acquire();
void test_binary_pool_release_then_reacquire();
void test_binary_pool_release_unacquired_is_noop();
void test_binary_pool_release_double_is_noop();
void test_binary_pool_release_before_acquire_is_safe();
void test_binary_pool_exhaust_then_release_all();
void test_binary_pool_reset_marks_all_free();
void test_binary_pool_reset_before_acquire_is_safe();
void test_binary_pool_count_one_pool();
void test_binary_pool_handle_resolve_and_get();
void test_binary_pool_handle_invalid_after_release();
void test_binary_pool_handle_rejects_recycled_slot();
void test_binary_pool_handle_stale_release_is_noop();
void test_binary_pool_handle_invalid_after_reset();
void test_binary_pool_handle_generation_wraps();

/* ===================== vec2 (test_vec.c) ===================== */
void test_vec2_constructors();
void test_vec2_mag();
void test_vec2_dot();
void test_vec2_unit();
void test_vec2_scale();
void test_vec2_add();
void test_vec2_sub();
void test_vec2_mul();
void test_vec2_div();
void test_vec2_lerp();

/* ===================== vec3 (test_vec.c) ===================== */
void test_vec3_constructors();
void test_vec3_mag();
void test_vec3_dot();
void test_vec3_unit();
void test_vec3_scale();
void test_vec3_add();
void test_vec3_sub();
void test_vec3_mul();
void test_vec3_div();
void test_vec3_lerp();

/* ===================== vec4 (test_vec.c) ===================== */
void test_vec4_constructors();
void test_vec4_mag();
void test_vec4_dot();
void test_vec4_unit();
void test_vec4_scale();
void test_vec4_add();
void test_vec4_sub();
void test_vec4_mul();
void test_vec4_div();
void test_vec4_lerp();

/* ===================== vecn (test_vecn.c) ===================== */
void test_vecn_f32_create();
void test_vecn_f32_create_null_items();
void test_vecn_f32_copy();
void test_vecn_f32_distance();
void test_vecn_f32_distance_equals_mag_of_sub();
void test_vecn_f32_cosine_distance();
void test_vecn_f32_cosine_distance_zero_vector();
void test_vecn_f32_unit();
void test_vecn_f32_scale();
void test_vecn_f32_add();
void test_vecn_f32_sub();
void test_vecn_f32_mul();
void test_vecn_f32_div();
void test_vecn_f32_mag_sq();
void test_vecn_f32_sum();
void test_vecn_f32_distance_sq();
void test_vecn_f32_manhattan_distance();
void test_vecn_f32_equals();
void test_vecn_f32_negate();
void test_vecn_f32_abs();
void test_vecn_f32_clamp();
void test_vecn_f32_lerp();
void test_vecn_f32_simd_paths();

/* ===================== matn (test_matn.c) ===================== */
void test_matn_f32_create();
void test_matn_f32_create_null_items();
void test_matn_f32_copy();
void test_matn_f32_identity();
void test_matn_f32_at_set();
void test_matn_f32_get_row();
void test_matn_f32_copy_row();
void test_matn_f32_equals();
void test_matn_f32_transpose();
void test_matn_f32_mul();
void test_matn_f32_mul_into();
void test_matn_f32_mul_vec();
void test_matn_f32_scale();
void test_matn_f32_negate();
void test_matn_f32_abs();
void test_matn_f32_clamp();
void test_matn_f32_lerp();
void test_matn_f32_add();
void test_matn_f32_sub();
void test_matn_f32_mul_elementwise();
void test_matn_f32_div();
void test_matn_f32_binop_into();
void test_matn_f32_unary_into();
void test_matn_f32_simd_paths();

/* ===================== noise (test_noise.c) ===================== */
void test_noise_value_2d_deterministic();
void test_noise_value_2d_range();
void test_noise_value_2d_continuity();
void test_noise_value_2d_seed_sensitivity();
void test_noise_perlin_2d_deterministic();
void test_noise_perlin_2d_range();
void test_noise_perlin_2d_zero_at_lattice();
void test_noise_perlin_2d_continuity();
void test_noise_perlin_2d_zero_mean();
void test_noise_perlin_2d_seed_sensitivity();
void test_noise_simplex_2d_deterministic();
void test_noise_simplex_2d_range();
void test_noise_simplex_2d_continuity();
void test_noise_simplex_2d_zero_mean();
void test_noise_simplex_2d_seed_sensitivity();
void test_noise_value_2d_fbm_single_octave_equals_base();
void test_noise_value_2d_fbm_range();
void test_noise_perlin_2d_fbm_single_octave_equals_base();
void test_noise_perlin_2d_fbm_range();
void test_noise_simplex_2d_fbm_single_octave_equals_base();
void test_noise_simplex_2d_fbm_range();

/* ===================== serial (test_serial.c) ===================== */
void test_serial_size_of_component();
void test_serial_raw_block_round_trip();
void test_serial_decode_scalar_array();
void test_serial_decode_scalar_single_f64();
void test_serial_decode_string();
void test_serial_decode_empty_string();
void test_serial_decode_nil_block();
void test_serial_decode_nested_blocks();
void test_serial_decode_empty_block_container();
void test_serial_decode_truncated_file_fails();
void test_serial_decode_size_length_mismatch_fails();
void test_serial_decode_sequential_blocks();

/* ===================== hash (test_hash.c) ===================== */
void test_hash_uuidv4_format();
void test_hash_uuidv4_deterministic();
void test_hash_uuidv4_distinct();
void test_hash_uuidv4_hex_uses_digits();

#endif
