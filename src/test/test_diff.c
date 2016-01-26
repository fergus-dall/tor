#include "or.h"
#include "container.h"
#include "diff.h"
#include "test.h"
#include "util.h"

static int
check_common_subsequence(smartlist_t *first, smartlist_t *second,
                         smartlist_t *cs)
{
  int *first_idxs = tor_malloc_zero((smartlist_len(cs)+2) * sizeof(int));
  int *second_idxs = tor_malloc_zero((smartlist_len(cs)+2) * sizeof(int));
  first_idxs[0] = -1; second_idxs[0] = -1;
  first_idxs[smartlist_len(cs)+1] = smartlist_len(first);
  second_idxs[smartlist_len(cs)+1] = smartlist_len(second);

  /* Check that all common elements occur in both lists in the correct
   * order */
  int cs_idx = 0;
  SMARTLIST_FOREACH_BEGIN(first, char *, fr) {
    if (cs_idx >= smartlist_len(cs))
      break;
    if (!strcmp(fr, smartlist_get(cs, cs_idx)))
      first_idxs[++cs_idx] = fr_sl_idx;
  } SMARTLIST_FOREACH_END(fr);
  tt_int_op(cs_idx, OP_EQ, smartlist_len(cs));

  cs_idx = 0;
  SMARTLIST_FOREACH_BEGIN(second, char *, se) {
    if (cs_idx >= smartlist_len(cs))
      break;
    if (!strcmp(se, smartlist_get(cs, cs_idx)))
      second_idxs[++cs_idx] = se_sl_idx;
  } SMARTLIST_FOREACH_END(se);
  tt_int_op(cs_idx, OP_EQ, smartlist_len(cs));

  /* We intentionally do not check that the subsequence is the longest
   * possible subsequence, because we use heuristics to speed things
   * up. Non-maximal subsequences can cause sub-optimal diffs, but
   * cannot cause incorrect diffs, so this is safe. */

  tor_free(first_idxs);
  tor_free(second_idxs);
  return 0;
done:
  tor_free(first_idxs);
  tor_free(second_idxs);
  return -1;
}

static void
test_diff_smartlist_longest_common_subsequence(void *arg)
{
  (void)arg;
  smartlist_t *first;
  smartlist_t *second;
  smartlist_t *result = NULL;

  tor_weak_rng_t rng;
  tor_init_weak_random(&rng, time(NULL));
  int n;
  for (n = 0; n < 100; n++) {
    first = smartlist_new();
    second = smartlist_new();
    int i;
    for (i = 0; i < 10; i++) {
      int j;
      for (j = tor_weak_random_range(&rng, 4); j > 0; j--) {
        smartlist_add_asprintf(first, "%x", tor_weak_random(&rng));
      }
      smartlist_add_asprintf(first, "%d", i);
      for (j = tor_weak_random_range(&rng, 4); j > 0; j--) {
        smartlist_add_asprintf(second, "%x", tor_weak_random(&rng));
      }
      smartlist_add_asprintf(second, "%d", i);
    }

    result = smartlist_longest_common_subsequence(first, second);
    if (check_common_subsequence(first, second, result))
      tt_abort();

    smartlist_free(result);
    SMARTLIST_FOREACH(first, char *, str, tor_free(str));
    smartlist_free(first);
    SMARTLIST_FOREACH(second, char *, str, tor_free(str));
    smartlist_free(second);
  }
  return;
done:
  smartlist_free(result);
  SMARTLIST_FOREACH(first, char *, str, tor_free(str));
  smartlist_free(first);
  SMARTLIST_FOREACH(second, char *, str, tor_free(str));
  smartlist_free(second);
}

static void
test_diff_add_next_command(void *arg)
{
  (void)arg;
  smartlist_t *diff_sl = smartlist_new();
  smartlist_t *new_sl = smartlist_new();

  smartlist_split_string(new_sl, "aaaa bbbb cccc dddd", " ", 0, 0);

  diff_add_next_command(diff_sl, 1, 0, new_sl, 1, 0);
  tt_int_op(smartlist_len(diff_sl), OP_EQ, 0);

  diff_add_next_command(diff_sl, 1, 1, new_sl, 1, 0);
  tt_int_op(smartlist_len(diff_sl), OP_EQ, 1);

  diff_add_next_command(diff_sl, 1, 4, new_sl, 1, 0);
  tt_int_op(smartlist_len(diff_sl), OP_EQ, 2);

  diff_add_next_command(diff_sl, 1, 0, new_sl, 1, 3);
  tt_int_op(smartlist_len(diff_sl), OP_EQ, 7);

  diff_add_next_command(diff_sl, 1, 1, new_sl, 0, 2);
  tt_int_op(smartlist_len(diff_sl), OP_EQ, 12);

  diff_add_next_command(diff_sl, 1, 4, new_sl, 1, 3);
  tt_int_op(smartlist_len(diff_sl), OP_EQ, 17);

  tt_str_op(smartlist_get(diff_sl, 0), OP_EQ, "2d");
  tt_str_op(smartlist_get(diff_sl, 1), OP_EQ, "2,5d");
  tt_str_op(smartlist_get(diff_sl, 2), OP_EQ, "1a");
  tt_str_op(smartlist_get(diff_sl, 3), OP_EQ, "bbbb");
  tt_str_op(smartlist_get(diff_sl, 4), OP_EQ, "cccc");
  tt_str_op(smartlist_get(diff_sl, 5), OP_EQ, "dddd");
  tt_str_op(smartlist_get(diff_sl, 6), OP_EQ, ".");
  tt_str_op(smartlist_get(diff_sl, 7), OP_EQ, "2c");
  tt_str_op(smartlist_get(diff_sl, 8), OP_EQ, "aaaa");
  tt_str_op(smartlist_get(diff_sl, 9), OP_EQ, "bbbb");
  tt_str_op(smartlist_get(diff_sl, 10), OP_EQ, "cccc");
  tt_str_op(smartlist_get(diff_sl, 11), OP_EQ, ".");
  tt_str_op(smartlist_get(diff_sl, 12), OP_EQ, "2,5c");
  tt_str_op(smartlist_get(diff_sl, 13), OP_EQ, "bbbb");
  tt_str_op(smartlist_get(diff_sl, 14), OP_EQ, "cccc");
  tt_str_op(smartlist_get(diff_sl, 15), OP_EQ, "dddd");
  tt_str_op(smartlist_get(diff_sl, 16), OP_EQ, ".");
done:
  SMARTLIST_FOREACH(diff_sl, char *, str, tor_free(str));
  smartlist_free(diff_sl);
  SMARTLIST_FOREACH(new_sl, char *, str, tor_free(str));
  smartlist_free(new_sl);
}

#define DIFF_OLD "some stuff that stays the same\n" \
    "some stuff that stays the same\n"              \
    "a line that is removed\n"                      \
    "some stuff that stays the same\n"              \
    "a block\n"                                     \
    "that is\n"                                     \
    "removed\n"                                     \
    "some stuff that stays the same\n"              \
    "some stuff that stays the same\n"              \
    "a block\n"                                     \
    "that is\n"                                     \
    "overwitten\n"                                  \
    "some stuff that stays the same\n"              \
    "a line that is overwitten\n"

#define DIFF_NEW "new thing at start\n"  \
    "some stuff that stays the same\n"   \
    "some stuff that stays the same\n"   \
    "some stuff that stays the same\n"   \
    "some stuff that stays the same\n"   \
    "a block that\n"                     \
    "is\n"                               \
    "inserted\n"                         \
    "some stuff that stays the same\n"   \
    "aaaaaa\n"                           \
    "bbbbbb\n"                           \
    "some stuff that stays the same\n"   \
    "cccccc\n"                           \
    "dddddd\n"

#define DIFF_PATCH "14c\n"                      \
    "cccccc\n"                                  \
    "dddddd\n"                                  \
    ".\n"                                       \
    "10,12c\n"                                  \
    "aaaaaa\n"                                  \
    "bbbbbb\n"                                  \
    ".\n"                                       \
    "8a\n"                                      \
    "a block that\n"                            \
    "is\n"                                      \
    "inserted\n"                                \
    ".\n"                                       \
    "5,7d\n"                                    \
    "3d\n"                                      \
    "0a\n"                                      \
    "new thing at start\n"                      \
    "."

static void
test_diff_make_diff(void *arg)
{
  (void)arg;
  char *diff = make_diff(DIFF_OLD, DIFF_NEW);
  tt_str_op(diff, OP_EQ, DIFF_PATCH);

done:
  tor_free(diff);
}

static void
test_diff_apply_patch(void *arg)
{
  (void)arg;
  char *new = apply_patch(DIFF_OLD, DIFF_PATCH);
  tt_str_op(new, OP_EQ, DIFF_NEW);

done:
  tor_free(new);
}

#undef DIFF_OLD
#undef DIFF_NEW
#undef DIFF_PATCH

#define DIFF_TEST(name, flags) \
  { #name, test_diff_ ##name, (flags), NULL, NULL }

struct testcase_t diff_tests[] = {
  DIFF_TEST(smartlist_longest_common_subsequence, 0),
  DIFF_TEST(add_next_command, 0),
  DIFF_TEST(make_diff, 0),
  DIFF_TEST(apply_patch, 0),
  END_OF_TESTCASES
};
