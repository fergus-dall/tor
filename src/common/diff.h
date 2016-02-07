#ifndef TOR_DIFF_H
#define TOR_DIFF_H

char *make_diff(const char *old, const char *new);
char *apply_patch(const char *base, const char *patch);
smartlist_t *smartlist_longest_common_subsequence(smartlist_t *first,
                                                  smartlist_t *second);
void smartlist_longest_common_subsequence_impl(
  smartlist_t *first, int first_start, int first_end,
  smartlist_t *second, int second_start, int second_end,
  int *fowards_v, int *reverse_v, int too_expensive,
  uint64_t *f_hash, uint64_t *s_hash, smartlist_t *result);
void diff_add_next_command(smartlist_t *diff_sl, int remove_start,
                           int remove_end, smartlist_t *new_sl,
                           int insert_start, int insert_end);

#endif
