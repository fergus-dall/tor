#include "container.h"
#include "diff.h"
#include "siphash.h"
#include "util.h"

/** Create an ed-style diff from <b>old</b> to <b>new</b> and return
 * it in a newly allocated string.
 **/
char *
make_diff(const char *old, const char *new)
{
  char *old_dup = tor_strdup(old);
  char *new_dup = tor_strdup(new);
  char *diff = NULL;
  smartlist_t *old_sl = smartlist_new();
  smartlist_t *new_sl = smartlist_new();
  smartlist_t *common_subsequence;
  smartlist_t *diff_sl = smartlist_new();

  tor_split_lines(old_sl, old_dup, strlen(old_dup));
  tor_split_lines(new_sl, new_dup, strlen(new_dup));

  common_subsequence = smartlist_longest_common_subsequence(old_sl, new_sl);
  smartlist_reverse(common_subsequence);

  int remove_start = smartlist_len(old_sl)-1;
  int remove_end = remove_start;
  int insert_start = smartlist_len(new_sl)-1;
  int insert_end = insert_start;
  SMARTLIST_FOREACH_BEGIN(common_subsequence, char *, cs) {
    while (strcmp(smartlist_get(old_sl, remove_start), cs))
      remove_start--;
    while (strcmp(smartlist_get(new_sl, insert_start), cs)) {
      insert_start--;
    }
    remove_start++; insert_start++;

    diff_add_next_command(diff_sl, remove_start, remove_end,
                     new_sl, insert_start, insert_end);

    remove_end = remove_start - 2; remove_start = remove_end;
    insert_end = insert_start - 2; insert_start = insert_end;
  } SMARTLIST_FOREACH_END(cs);

  /* Handle changes at the start of a file */
  diff_add_next_command(diff_sl, 0, remove_end, new_sl, 0, insert_end);

  diff = smartlist_join_strings(diff_sl, "\n", 0, NULL);
  
  smartlist_free(old_sl);
  smartlist_free(new_sl);
  smartlist_free(common_subsequence);
  SMARTLIST_FOREACH(diff_sl, char *, str, tor_free(str));
  smartlist_free(diff_sl);
  tor_free(old_dup);
  tor_free(new_dup);

  return diff;
}

/** Add the next diff command to <b>diff_sl</b>. This command removes
 * the lines from <b>remove_start</b> to <b>remove_end</b> inclusive
 * and inserts the lines in <b>new_sl</b> from <b>insert_start</b> to
 * <b>insert_end</b> inclusive.
 **/
void
diff_add_next_command(smartlist_t *diff_sl, int remove_start, int remove_end,
                      smartlist_t *new_sl, int insert_start, int insert_end)
{

  if (insert_start <= insert_end) {
    if (remove_start < remove_end) {
      /* Inserting some number of lines over a block */
      smartlist_add_asprintf(diff_sl, "%d,%dc", remove_start+1, remove_end+1);
    } else if (remove_start == remove_end) {
      /* Inserting some number of lines over a single line */
      smartlist_add_asprintf(diff_sl, "%dc", remove_start+1);
    } else {
      /* Inserting some number of lines without removing anything */
      smartlist_add_asprintf(diff_sl, "%da", remove_end+1);
    }
    int idx;
    for (idx = insert_start; idx <= insert_end; idx++) {
      char *temp = smartlist_get(new_sl, idx);
      smartlist_add(diff_sl, tor_strdup(temp));
    }
    smartlist_add(diff_sl, tor_strdup("."));
  } else {
    if (remove_start < remove_end) {
      /* Removing a block */
      smartlist_add_asprintf(diff_sl, "%d,%dd", remove_start+1, remove_end+1);
    } else if (remove_start == remove_end) {
      /* Removing a single line */
      smartlist_add_asprintf(diff_sl, "%dd", remove_start+1);
    } else {
      /* We are in the middle of an unchanged block, so do nothing */
    }
  }
}

/** Compute the longest common subsequence of two smartlists of
 * NULL-terminated strings and return the result as a new
 * smartlist.
 **/

smartlist_t *
smartlist_longest_common_subsequence(smartlist_t *first, smartlist_t *second) {
  int max = (smartlist_len(first) + smartlist_len(second))/2 + 1;
  int *forward_v = tor_malloc_zero(max*2 * sizeof(int));
  int *reverse_v = tor_malloc_zero(max*2 * sizeof(int));

  /* Take hashes of all the strings so we don't have to do comparisons
   * with strcmp */
  uint64_t *f_hash = tor_malloc_zero(smartlist_len(first) * 8);
  uint64_t *s_hash = tor_malloc_zero(smartlist_len(second) * 8);
  SMARTLIST_FOREACH(first, char *, str,
                    f_hash[str_sl_idx] = siphash24g(str, strlen(str)));
  SMARTLIST_FOREACH(second, char *, str,
                    s_hash[str_sl_idx] = siphash24g(str, strlen(str)));

  /** Use the TOO_EXPENSIVE heuristic from GNU difftools, which gives
   * up after an edit distance of the approximate square root of the
   * total file lengths */
  int too_expensive = 1;
  int total_length = smartlist_len(first) + smartlist_len(second);
  for (; total_length != 0; total_length >>= 2)
    too_expensive <<= 1;
  if (too_expensive < 256)
    too_expensive = 256;

  smartlist_t *result = smartlist_new();
  smartlist_longest_common_subsequence_impl(first, 0, smartlist_len(first),
                                            second, 0, smartlist_len(second),
                                            forward_v, reverse_v,
                                            too_expensive, f_hash, s_hash,
                                            result);

  tor_free(forward_v);
  tor_free(reverse_v);
  tor_free(f_hash);
  tor_free(s_hash);
  return result;
}

/** 
 **/
void
smartlist_longest_common_subsequence_impl(
  smartlist_t *first, int first_start, int first_end,
  smartlist_t *second, int second_start, int second_end,
  int *forward_v, int *reverse_v, int too_expensive,
  uint64_t *f_hash, uint64_t *s_hash, smartlist_t *result)
{
  if (first_end - first_start <= 0 || second_end - second_start <= 0)
    return;

  int delta_start = first_start - second_start;
  int delta_end = first_end - second_end;
  int delta = delta_end - delta_start;
  int max = (first_end - first_start + second_end - second_start)/2 + 1;
  if (max > too_expensive)
    max = too_expensive;
  int center_forward = max - delta_start;
  int center_reverse = max - delta_end;
  forward_v[max+1] = first_start;
  reverse_v[max+1] = first_end+1;
  int diff_size;
  int x, y; /* Start of the middle snake */
  int u, v; /* End of the middle snake */
  int found_middle_snake = 0;
  int diagonal;
  for (diff_size = 0; diff_size <= max && !found_middle_snake; diff_size++) {
    for (diagonal = delta_start - diff_size;
         diagonal <= delta_start + diff_size && !found_middle_snake;
         diagonal += 2) {
      if (diagonal == delta_start - diff_size ||
          (diagonal != delta_start + diff_size
           && forward_v[center_forward+diagonal-1] < forward_v[center_forward+diagonal+1])) {
        x = forward_v[center_forward+diagonal+1];
      } else {
        x = forward_v[center_forward+diagonal-1]+1;
      }
      y = x-diagonal;
      u = x;
      v = y;
      while (u < first_end && v < second_end && f_hash[u] == s_hash[v]) {
        u++;
        v++;
      }
      forward_v[center_forward+diagonal] = u;

      if (delta % 2 && diagonal >= delta_end - (diff_size-1)
          && diagonal <= delta_end + (diff_size-1)
          && forward_v[diagonal+center_forward] >= reverse_v[diagonal+center_reverse]) {
        found_middle_snake = 1;
      }
    }
    for (diagonal = delta_end - diff_size;
         diagonal <= delta_end + diff_size && !found_middle_snake;
         diagonal += 2) {
      if (diagonal == delta_end - diff_size ||
          (diagonal != delta_end + diff_size
           && reverse_v[center_reverse+diagonal-1] >= reverse_v[center_reverse+diagonal+1])) {
        u = reverse_v[center_reverse+diagonal+1]-1;
      } else {
        u = reverse_v[center_reverse+diagonal-1];
      }
      v = u-diagonal;
      x = u;
      y = v;
      while (x > first_start && y > second_start &&
             f_hash[x-1] == s_hash[y-1]) {
        x--;
        y--;
      }
      reverse_v[center_reverse+diagonal] = x;

      if (delta % 2 == 0 && diagonal >= delta_start - diff_size
          && diagonal <= delta_start + diff_size
          && forward_v[diagonal+center_forward] >= reverse_v[diagonal+center_reverse]) {
        found_middle_snake = 1;
      }
    }
  }
  diff_size--;

  if (!found_middle_snake) {
    int f_best_x = 0;
    int f_best_y = 0;
    for (diagonal = delta_start - diff_size;
         diagonal <= delta_start + diff_size; diagonal += 2) {
      x = forward_v[center_forward+diagonal];
      y = x-diagonal;
      if (x + y > f_best_x + f_best_y) {
        f_best_x = x;
        f_best_y = y;
      }
    }

    int r_best_x = first_end;
    int r_best_y = second_end;
    for (diagonal = delta_end - diff_size;
         diagonal <= delta_end + diff_size; diagonal += 2) {
      x = reverse_v[center_reverse+diagonal];
      y = x-diagonal;
      if (x + y < r_best_x + r_best_y) {
        r_best_x = x;
        r_best_y = y;
      }
    }

    if ((f_best_x - first_start) + (f_best_y - second_start) >
        (first_end - r_best_x) + (second_end - r_best_y)) {
      x = f_best_x;
      y = f_best_y;
      u = x;
      v = y;
    } else {
      x = r_best_x;
      y = r_best_y;
      u = x;
      v = y;
    }
  }

  int i;
  if (diff_size > 1 || (delta%2 == 0 && diff_size == 1)) {
    smartlist_longest_common_subsequence_impl(first, first_start, x,
                                              second, second_start, y,
                                              forward_v, reverse_v,
                                              too_expensive, f_hash, s_hash,
                                              result);
    for (i = x; i < u; i++) {
      smartlist_add(result, smartlist_get(first, i));
    }
    smartlist_longest_common_subsequence_impl(first, u, first_end,
                                              second, v, second_end,
                                              forward_v, reverse_v,
                                              too_expensive, f_hash, s_hash,
                                              result);
  } else if (first_end - first_start < second_end - second_start) {
    for (i = first_start; i < first_end; i++) {
      smartlist_add(result, smartlist_get(first, i));
    }
  } else {
    for (i = second_start; i < second_end; i++) {
      smartlist_add(result, smartlist_get(second, i));
    }
  }
}

/** Apply the ed-style diff in <b>diff</b> to <b>base</b> and return
 * the result in a newly allocated string.
 **/
char *
apply_patch(const char *base, const char *diff)
{
  char *base_dup = tor_strdup(base);
  char *diff_dup = tor_strdup(diff);
  char *new = NULL;
  smartlist_t *base_sl = smartlist_new();
  smartlist_t *diff_sl = smartlist_new();

  tor_split_lines(base_sl, base_dup, strlen(base_dup));
  tor_split_lines(diff_sl, diff_dup, strlen(diff_dup));

  int idx;
  for (idx = 0; idx < smartlist_len(diff_sl); idx++) {
    char *str = smartlist_get(diff_sl, idx);
    int start = atoi(str);
    int end;
    if (strchr(str, ',')) {
      end = atoi(strchr(str, ',') + 1);
    } else {
      end = start;
    }
    char command = str[strlen(str) - 1];

    if (command == 'c' || command == 'd') {
      for (; end > start-1; end--) {
        smartlist_del_keeporder(base_sl, start-1);
      }
    }
    if (command == 'c')
      start--;
    if (command == 'c' || command == 'a') {
      idx++;
      str = smartlist_get(diff_sl, idx);
      while (strcmp(str, ".")) {
        smartlist_insert(base_sl, start, str);
        idx++; start++;
        str = smartlist_get(diff_sl, idx);
      }
    }
  }

  new = smartlist_join_strings(base_sl, "\n", 1, NULL);
  
  smartlist_free(base_sl);
  smartlist_free(diff_sl);
  tor_free(base_dup);
  tor_free(diff_dup);

  return new;
}
