#include <test/tests.h>
#include <kl/cli/args.h>


static void run_test(KL_TestEntry* entry) {
  KL_TestFunctionContext ctx = {0};
  ctx.current_suite_name = entry->name;
  g_current_ctx = &ctx;
  KL_TEST_BEGIN(entry->name);
  entry->fn(&ctx);
  KL_TEST_END(entry->name);
  g_current_ctx = NULL;
}

int main(int argc, char* argv[]) {
  KL_TestEntry tests[] = {
    KL_TEST_ENTRY(test_str_const_slice),
    KL_TEST_ENTRY(test_str_const_includes),
    KL_TEST_ENTRY(test_str_scanner),

    KL_TEST_ENTRY(test_json_tokenizer),
    KL_TEST_ENTRY(test_json_parser),
    KL_TEST_ENTRY(test_json_node_utils),

    KL_TEST_ENTRY(test_allocator_init_destroy),
    KL_TEST_ENTRY(test_allocator_double_init_is_noop),
    KL_TEST_ENTRY(test_allocator_basic_malloc),
    KL_TEST_ENTRY(test_allocator_zero_initialized_memory),
    KL_TEST_ENTRY(test_allocator_multiple_mallocs_no_overlap),
    KL_TEST_ENTRY(test_allocator_new_page_growth),
    KL_TEST_ENTRY(test_allocator_large_alloc_grows_page),
    KL_TEST_ENTRY(test_allocator_malloc_aligned),
    KL_TEST_ENTRY(test_allocator_strdup),
    KL_TEST_ENTRY(test_allocator_sprintf),
    KL_TEST_ENTRY(test_allocator_reset_zeros_and_keeps_pages),
    KL_TEST_ENTRY(test_allocator_reset_grows_capacity),
    KL_TEST_ENTRY(test_allocator_realloc_grow_preserves_data),
    KL_TEST_ENTRY(test_allocator_realloc_shrink_returns_same_pointer),
    KL_TEST_ENTRY(test_allocator_realloc_from_zero_acts_as_malloc),
    KL_TEST_ENTRY(test_allocator_tracked_malloc_records_allocations),
    KL_TEST_ENTRY(test_allocator_free_zeros_memory_and_reuses_slot),
    KL_TEST_ENTRY(test_allocator_save_restore_from_empty),
    KL_TEST_ENTRY(test_allocator_save_restore_preserves_prior_data),
    KL_TEST_ENTRY(test_allocator_save_restore_drops_new_pages),
    KL_TEST_ENTRY(test_allocator_destroy_all_pages_keeps_allocator_usable),
    KL_TEST_ENTRY(test_allocator_many_small_allocs),
    KL_TEST_ENTRY(test_allocator_strdup_in_reused_page),

    KL_TEST_ENTRY(test_allocator_tracked_realloc_grow_preserves_data),
    KL_TEST_ENTRY(test_allocator_tracked_realloc_grow_marks_old_freed),
    KL_TEST_ENTRY(test_allocator_tracked_realloc_grow_zeros_old_region),
    KL_TEST_ENTRY(test_allocator_tracked_realloc_old_slot_reusable),
    KL_TEST_ENTRY(test_allocator_tracked_realloc_new_region_is_freeable),
    KL_TEST_ENTRY(test_allocator_tracked_realloc_shrink_preserves_pointer_and_tracking),
    KL_TEST_ENTRY(test_allocator_tracked_realloc_from_zero_tracks_new_region),
    KL_TEST_ENTRY(test_allocator_tracked_realloc_aligned),
    KL_TEST_ENTRY(test_allocator_tracked_realloc_chain),
    KL_TEST_ENTRY(test_allocator_tracked_realloc_reuses_pre_freed_slot),
    KL_TEST_ENTRY(test_allocator_tracked_realloc_many_independent_handles),

    KL_TEST_ENTRY(test_int64_list_init_destroy),
    KL_TEST_ENTRY(test_int64_list_double_init_is_noop),
    KL_TEST_ENTRY(test_int64_list_push_basic),
    KL_TEST_ENTRY(test_int64_list_push_many_preserves_order),
    KL_TEST_ENTRY(test_int64_list_push_grows_capacity),
    KL_TEST_ENTRY(test_int64_list_grow_increases_slots),
    KL_TEST_ENTRY(test_int64_list_grow_zeros_new_slots),
    KL_TEST_ENTRY(test_int64_list_pop_basic),
    KL_TEST_ENTRY(test_int64_list_pop_lifo_order),
    KL_TEST_ENTRY(test_int64_list_pop_with_null_out),
    KL_TEST_ENTRY(test_int64_list_pop_empty_returns_zero),
    KL_TEST_ENTRY(test_int64_list_pop_at_first),
    KL_TEST_ENTRY(test_int64_list_pop_at_middle),
    KL_TEST_ENTRY(test_int64_list_pop_at_last),
    KL_TEST_ENTRY(test_int64_list_pop_at_empty_returns_zero),
    KL_TEST_ENTRY(test_int64_list_push_start_basic),
    KL_TEST_ENTRY(test_int64_list_push_start_reverses_insertion_order),
    KL_TEST_ENTRY(test_int64_list_push_start_after_push),
    KL_TEST_ENTRY(test_int64_list_push_start_grows_when_needed),
    KL_TEST_ENTRY(test_int64_list_includes_found),
    KL_TEST_ENTRY(test_int64_list_includes_not_found),
    KL_TEST_ENTRY(test_int64_list_includes_empty),
    KL_TEST_ENTRY(test_int64_list_includes_after_pop),
    KL_TEST_ENTRY(test_int64_list_reset_clears_length),
    KL_TEST_ENTRY(test_int64_list_reset_zeros_items),
    KL_TEST_ENTRY(test_int64_list_reset_allows_reuse),
    KL_TEST_ENTRY(test_int64_list_reset_on_empty_is_safe),
    KL_TEST_ENTRY(test_int64_list_with_arena_allocator),
    KL_TEST_ENTRY(test_int64_list_arena_grow_path),
    KL_TEST_ENTRY(test_int64_list_push_pop_interleaved),
    KL_TEST_ENTRY(test_int64_list_handles_negative_and_extreme_values),
    KL_TEST_ENTRY(test_int64_list_stress_many_elements),
    KL_TEST_ENTRY(test_int64_list_pop_at_drains_from_front),

    KL_TEST_ENTRY(test_int64_hashmap_init_destroy),
    KL_TEST_ENTRY(test_int64_hashmap_double_init_is_noop),
    KL_TEST_ENTRY(test_int64_hashmap_capacity_rounds_to_power_of_two),
    KL_TEST_ENTRY(test_int64_hashmap_set_get_basic),
    KL_TEST_ENTRY(test_int64_hashmap_get_missing_returns_zero),
    KL_TEST_ENTRY(test_int64_hashmap_set_overwrite_updates_value),
    KL_TEST_ENTRY(test_int64_hashmap_set_returns_valid_slot_index),
    KL_TEST_ENTRY(test_int64_hashmap_has_before_and_after),
    KL_TEST_ENTRY(test_int64_hashmap_unset_removes_entry),
    KL_TEST_ENTRY(test_int64_hashmap_unset_missing_returns_zero),
    KL_TEST_ENTRY(test_int64_hashmap_unset_then_reset_reinserts),
    KL_TEST_ENTRY(test_int64_hashmap_get_ptr_allows_mutation),
    KL_TEST_ENTRY(test_int64_hashmap_get_ptr_missing_returns_zero),
    KL_TEST_ENTRY(test_int64_hashmap_hash_is_deterministic),
    KL_TEST_ENTRY(test_int64_hashmap_collisions_with_tiny_capacity),
    KL_TEST_ENTRY(test_int64_hashmap_many_entries_round_trip),
    KL_TEST_ENTRY(test_int64_hashmap_unset_in_collision_chain),
    KL_TEST_ENTRY(test_int64_hashmap_string_keys_fnv_path),
    KL_TEST_ENTRY(test_int64_hashmap_keys_with_same_prefix_differ),
    KL_TEST_ENTRY(test_int64_hashmap_get_keys_lists_all_set_keys),
    KL_TEST_ENTRY(test_int64_hashmap_get_keys_excludes_unset),
    KL_TEST_ENTRY(test_int64_hashmap_used_slots_tracking),
    KL_TEST_ENTRY(test_int64_hashmap_get_used_slots_returns_set_only),
    KL_TEST_ENTRY(test_int64_hashmap_reset_clears_entries),
    KL_TEST_ENTRY(test_int64_hashmap_serialize_deserialize_round_trip),
    KL_TEST_ENTRY(test_int64_hashmap_deserialize_rejects_bad_magic),
    KL_TEST_ENTRY(test_int64_hashmap_zero_value_is_distinguishable),
    KL_TEST_ENTRY(test_int64_hashmap_negative_and_extreme_keys),

    KL_TEST_ENTRY(test_binary_sparseset_init_destroy),
    KL_TEST_ENTRY(test_binary_sparseset_double_init_is_noop),
    KL_TEST_ENTRY(test_binary_sparseset_set_get_basic),
    KL_TEST_ENTRY(test_binary_sparseset_get_missing_returns_null),
    KL_TEST_ENTRY(test_binary_sparseset_set_null_data_zero_inits),
    KL_TEST_ENTRY(test_binary_sparseset_set_overwrite_keeps_length),
    KL_TEST_ENTRY(test_binary_sparseset_set_returns_pointer_into_values),
    KL_TEST_ENTRY(test_binary_sparseset_has_before_and_after_set),
    KL_TEST_ENTRY(test_binary_sparseset_has_out_of_range_returns_false),
    KL_TEST_ENTRY(test_binary_sparseset_get_oob_returns_null),
    KL_TEST_ENTRY(test_binary_sparseset_remove_missing_returns_false),
    KL_TEST_ENTRY(test_binary_sparseset_remove_last_element_no_swap),
    KL_TEST_ENTRY(test_binary_sparseset_remove_middle_swaps_with_last),
    KL_TEST_ENTRY(test_binary_sparseset_remove_then_reinsert_same_id),
    KL_TEST_ENTRY(test_binary_sparseset_remove_all_drains),
    KL_TEST_ENTRY(test_binary_sparseset_reset_clears_entries),
    KL_TEST_ENTRY(test_binary_sparseset_reset_preserves_capacity),
    KL_TEST_ENTRY(test_binary_sparseset_reset_on_empty_is_safe),
    KL_TEST_ENTRY(test_binary_sparseset_sparse_grows_for_large_id),
    KL_TEST_ENTRY(test_binary_sparseset_dense_grows_when_full),
    KL_TEST_ENTRY(test_binary_sparseset_at_iterates_dense_buffer),
    KL_TEST_ENTRY(test_binary_sparseset_struct_payload_round_trip),
    KL_TEST_ENTRY(test_binary_sparseset_many_inserts_round_trip),
    KL_TEST_ENTRY(test_binary_sparseset_alignment_default),
    KL_TEST_ENTRY(test_binary_sparseset_id_zero_works),

    KL_TEST_ENTRY(test_fs_read_directory_rejects_bad_args),
    KL_TEST_ENTRY(test_fs_read_directory_lists_project_src_non_recursive),
    KL_TEST_ENTRY(test_fs_read_directory_recursive_descends_into_subdirs),

    KL_TEST_ENTRY(test_path_is_absolute),
    KL_TEST_ENTRY(test_path_dirname),
    KL_TEST_ENTRY(test_path_basename),
    KL_TEST_ENTRY(test_path_extname),
    KL_TEST_ENTRY(test_path_join),
    KL_TEST_ENTRY(test_path_normalize),
    KL_TEST_ENTRY(test_path_resolve),
    KL_TEST_ENTRY(test_path_cwd),

    KL_TEST_ENTRY(test_cli_get_arg_space_syntax),
    KL_TEST_ENTRY(test_cli_get_arg_equals_syntax),
    KL_TEST_ENTRY(test_cli_get_arg_quoted_values),
  };

  int64_t n_tests = KL_ARRAY_LEN(tests);

  KL_StrConst test_to_run = kl_cli_get_arg(argc, argv, KL_STR_CONST_LIT("--test"), false);

  if (test_to_run.value != 0) {
    bool found = false;
    for (int64_t i = 0; i < n_tests; i++) {
      KL_TestEntry* entry = &tests[i];
      if (kl_str_const_eq(test_to_run, KL_STR_CONST(entry->name))) {
        run_test(entry);
        found = true;
        break;
      }
    }
    if (!found) {
      fprintf(stderr, "No such test: '%s'\n", test_to_run.value);
      exit(1);
    }
  } else {
    for (int64_t i = 0; i < n_tests; i++) {
      KL_TestEntry* entry = &tests[i];
      run_test(entry);
    }
  }

  kl_test_print_summary();
  return 0;
}
