/*
#define DEBUG
 */

#include "nip4.h"

/* Minimum value of three values.
 */
#define MIN3(a, b, c) \
	((a) < (b) ? ((a) < (c) ? (a) : (c)) : ((b) < (c) ? (b) : (c)))

/* LEVENSHTEIN DISTANCE
 *
 * @s1 and @s2 are character arrays of size @n1 and @n2, which represent two
 * strings. Any null bytes are treated like a normal character, so strings
 * without a terminating null byte are valid input.
 *
 * The LEVENSHTEIN DISTANCE between @s1 and @s2 is the smallest number of
 * insertions, deletions, or substitutions needed to transform @s1 into @s2.
 *
 * The algorithm dynamically builds a table T[n1+1]][n2+1] where the (x, y)
 * entry is the LEVENSHTEIN DISTANCE between the prefix of @s1 of size y-1 and
 * the prefix of @s2 of size x-1.
 *
 * This implementation uses only a single @v column, updating it in place,
 * since only the elements at (x-1, y-1) and (x, y-1) are needed to compute
 * the element at (x, y). When the three costs defined by this dynamic
 * algorithm are equal to 1, this is expressed by the recursion relation:
 *
 *	k = (s1[y-1] == s2[x-2] ? 0 : 1)
 *
 * 	T[x][y] = MIN3( T[x-1][y] + 1, T[x][y - 1] + 1, T[x-1][y-1] + k )
 *
 * If we let @t1 hold the previous diagonal value, we can rewrite this
 * in terms of the single column @v:
 *
 *	k = (s1[y-1] == s2[x-2] ? 0 : 1)
 *
 * 	v[y] = MIN3( v[y] + 1, v[y-1] + 1, t1 + k )
 *
 * This array @v is reusable, so it can be allocated beforehand.
 *
 * The first column of the table T is unused, and the first row is
 * used as the base case for the recursion relation.
 *
 *                         s2
 *
 *     *----l--e--v--e--n--s--h--t--e--i--n--- x
 *     | .  1  2  3  4  5  6  7  8  9  10 11
 *     |
 *     d .  1  2  3  4  5  6  7  8  9  10 11
 *     |
 *     a .  2  2  3  4  5  6  7  8  9  10 11
 *     |
 *     m .  3  3  3  4  5  6  7  8  9  10 11
 * s1  |
 *     e .  4  3  4  3  4  5  6  7  8  9  10
 *     |
 *     r .  5  4  4  4  4  5  6  7  8  9  10
 *     |
 *     a .  6  5  5  5  5  5  6  7  8  9  10
 *     |
 *     u .  7  6  6  6  6  6  6  7  8  9  10
 *     |
 *
 *     y
 *
 * SYMBOLS
 *
 * @v:	Column vector. Reused and updated in place. The @t1 and @t0 values
 *  keep track of the most recent and second most recent diagonal values.
 * @x:	Row Index.
 * @y:	Column Index.
 * @t0:	Second most recent diagonal value.
 * @t1:	Most recent diagonal value
 * @k:	Test inqeuality in s1[y-1] and s2[x-1], the pair of characters
 * 	corresponding to the table entry T[x][y]. Equals 0 if these characters
 *	are equal, and 1 if they are different.
 *
 * TERMS
 *
 * LD: LEVENSHTEIN DISTANCE
 *
 * SOURCES
 *
 * https://en.wikipedia.org/wiki/Levenshtein_distance
 * https://en.wikibooks.org/wiki/Algorithm_Implementation/Strings/Levenshtein_distance#C
 */

/* Compute the LEVENSHTEIN DISTANCE (LD) between two strings, @s1 and @s2.
 *
 * @n1:			Character array 1 size
 *
 * @s1:			Character array 1
 *
 * @n2:			Character array 2 size
 *
 * @s2:			Character array 2
 *
 * @v:			Buffer where column of LEVENSHTEIN DISTANCE values are
 * 				written during computation. Must be allocated
 * 				beforehand by the caller. Owned by the caller.
 *
 * @ignore_case:	TRUE if case is ignored.
 * 					FALSE if case is NOT ignored.
 */

/* The insertion, deletion, and substitution costs are set to allow for long
 * strings containing matches that are only a few deletions and/or
 * substitutions away from any position. There is 0 cost for inserting
 * characters to shift the smaller pattern toward the end in order to almost
 * match with a substring near the end of the longer string.
 *
 * This is ideal for the use case in the Properties widget (the only use
 * case), where it is used to suggest inexact matches as a user types,
 * whenever no exact matches are available for the current search pattern.
 *
 * The point is to be forgiving. The costs are not high for deletion and
 * insertion, but insertion cost is zero. The cost of swapping two
 * adjacent letters is DEL_COST + INS_COST = 1 + 0 = 1, so the costs of
 * all common mistakes are balanced except insertion, which is free.
 *
 * This allows the user to type quickly without worrying about small mistakes.
 *
 * The costs can be tweaked for other use cases if they arise.
 */
#define INS_COST 0
#define DEL_COST 1
#define SUB_COST 1

#define LOWER(C) (ignore_case ? g_ascii_tolower(C) : (C))

static guint
glev(guint n1, const gchar s1[n1],
	guint n2, const gchar s2[n2],
	guint v[n1 + 1], gboolean ignore_case)
{
	guint x, y, t0, t1, k;

	// Initialize the column.
	for (y = 1; y <= n1; y++)
		v[y] = y;

	// Ignore and don't even bother to initialize the first column. Walk
	// through columns after the first.
	for (x = 1; x <= n2; x++) {
		// The first row (ignoring the first entry) is just the column
		// indices from 1 to n2.
		v[0] = x;

		// The recursion relation defined above and the base case
		// conditions { y = 1, t1 = x - 1 } are used to build the
		// Table T column by column. Only one column @v is needed
		// in memory at a time, and it can be operated on in
		// place, as long as temporary variables @t0 and @t1 are used
		// to keep track of the last diagonal when @v is updated.
		for (y = 1, t1 = x - 1; y <= n1; y++) {
			t0 = v[y];

			k = LOWER(s1[y - 1]) == LOWER(s2[x - 1]) ? 0 : SUB_COST;

			v[y] = MIN3(v[y] + INS_COST, v[y - 1] + DEL_COST, t1 + k);
			t1 = t0;
		}
	}

	return v[y - 1];
}

static int
fuzzy_match_sort(const void *client1, const void *client2)
{
	const Fuzzy *a = (const Fuzzy *) client1;
	const Fuzzy *b = (const Fuzzy *) client2;

	return a->distance - b->distance;
}

/* Search a NULL-terminated array of strings for fuzzy matches to a search
 * string, sort the results by Levenshtein distance.
 *
 * Free the result with g_slist_free_full (g_steal_pointer (&fuzzy), g_free)
 */
GSList *
fuzzy_match(char **fields, const char *pattern)
{
	int n_pattern = strlen(pattern);

	// the int array we use to compute distance
	g_autofree guint *v = VIPS_ARRAY(NULL, n_pattern + 1, guint);

	GSList *matches = NULL;

	for (char **p = fields; *p; p++) {
		Fuzzy *fuzzy = g_new(Fuzzy, 1);
		const char *field = *p;

		fuzzy->field = field;
		fuzzy->distance =
			glev(n_pattern, pattern, strlen(field), field, v, TRUE);
		matches = g_slist_append(matches, fuzzy);
	}

	matches = g_slist_sort(matches, fuzzy_match_sort);

	return matches;
}
