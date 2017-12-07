#include "win_port.h"

#include <stdint.h>
#include <limits.h>
#include <string.h>

#include <direct.h>

/* win mkdir */
int mkdir_m(const char *p, mode_t mask)
{
	return _mkdir(p);
}

/* memmem function */
// eglibc 2.10
#if CHAR_BIT < 10
# define LONG_NEEDLE_THRESHOLD 32U
#else
# define LONG_NEEDLE_THRESHOLD SIZE_MAX
#endif

#define RETURN_TYPE void *
#define AVAILABLE(h, h_l, j, n_l) ((j) <= (h_l) - (n_l))
#ifndef MAX
# define MAX(a, b) ((a < b) ? (b) : (a))
#endif

#ifndef CANON_ELEMENT
# define CANON_ELEMENT(c) c
#endif
#ifndef CMP_FUNC
# define CMP_FUNC memcmp
#endif

#ifndef _LIBC
#define __builtin_expect(expr, val)   (expr)
#endif

static size_t
critical_factorization(const unsigned char *needle, size_t needle_len,
	size_t *period)
{
	/* Index of last byte of left half, or SIZE_MAX.  */
	size_t max_suffix, max_suffix_rev;
	size_t j; /* Index into NEEDLE for current candidate suffix.  */
	size_t k; /* Offset into current period.  */
	size_t p; /* Intermediate period.  */
	unsigned char a, b; /* Current comparison bytes.  */

						/* Invariants:
						0 <= j < NEEDLE_LEN - 1
						-1 <= max_suffix{,_rev} < j (treating SIZE_MAX as if it were signed)
						min(max_suffix, max_suffix_rev) < global period of NEEDLE
						1 <= p <= global period of NEEDLE
						p == global period of the substring NEEDLE[max_suffix{,_rev}+1...j]
						1 <= k <= p
						*/

						/* Perform lexicographic search.  */
	max_suffix = SIZE_MAX;
	j = 0;
	k = p = 1;
	while (j + k < needle_len)
	{
		a = CANON_ELEMENT(needle[j + k]);
		b = CANON_ELEMENT(needle[max_suffix + k]);
		if (a < b)
		{
			/* Suffix is smaller, period is entire prefix so far.  */
			j += k;
			k = 1;
			p = j - max_suffix;
		}
		else if (a == b)
		{
			/* Advance through repetition of the current period.  */
			if (k != p)
				++k;
			else
			{
				j += p;
				k = 1;
			}
		}
		else /* b < a */
		{
			/* Suffix is larger, start over from current location.  */
			max_suffix = j++;
			k = p = 1;
		}
	}
	*period = p;

	/* Perform reverse lexicographic search.  */
	max_suffix_rev = SIZE_MAX;
	j = 0;
	k = p = 1;
	while (j + k < needle_len)
	{
		a = CANON_ELEMENT(needle[j + k]);
		b = CANON_ELEMENT(needle[max_suffix_rev + k]);
		if (b < a)
		{
			/* Suffix is smaller, period is entire prefix so far.  */
			j += k;
			k = 1;
			p = j - max_suffix_rev;
		}
		else if (a == b)
		{
			/* Advance through repetition of the current period.  */
			if (k != p)
				++k;
			else
			{
				j += p;
				k = 1;
			}
		}
		else /* a < b */
		{
			/* Suffix is larger, start over from current location.  */
			max_suffix_rev = j++;
			k = p = 1;
		}
	}

	/* Choose the longer suffix.  Return the first byte of the right
	half, rather than the last byte of the left half.  */
	if (max_suffix_rev + 1 < max_suffix + 1)
		return max_suffix + 1;
	*period = p;
	return max_suffix_rev + 1;
}

static RETURN_TYPE
two_way_short_needle(const unsigned char *haystack, size_t haystack_len,
	const unsigned char *needle, size_t needle_len)
{
	size_t i; /* Index into current byte of NEEDLE.  */
	size_t j; /* Index into current window of HAYSTACK.  */
	size_t period; /* The period of the right half of needle.  */
	size_t suffix; /* The index of the right half of needle.  */

				   /* Factor the needle into two halves, such that the left half is
				   smaller than the global period, and the right half is
				   periodic (with a period as large as NEEDLE_LEN - suffix).  */
	suffix = critical_factorization(needle, needle_len, &period);

	/* Perform the search.  Each iteration compares the right half
	first.  */
	if (CMP_FUNC(needle, needle + period, suffix) == 0)
	{
		/* Entire needle is periodic; a mismatch can only advance by the
		period, so use memory to avoid rescanning known occurrences
		of the period.  */
		size_t memory = 0;
		j = 0;
		while (AVAILABLE(haystack, haystack_len, j, needle_len))
		{
			/* Scan for matches in right half.  */
			i = MAX(suffix, memory);
			while (i < needle_len && (CANON_ELEMENT(needle[i])
				== CANON_ELEMENT(haystack[i + j])))
				++i;
			if (needle_len <= i)
			{
				/* Scan for matches in left half.  */
				i = suffix - 1;
				while (memory < i + 1 && (CANON_ELEMENT(needle[i])
					== CANON_ELEMENT(haystack[i + j])))
					--i;
				if (i + 1 < memory + 1)
					return (RETURN_TYPE)(haystack + j);
				/* No match, so remember how many repetitions of period
				on the right half were scanned.  */
				j += period;
				memory = needle_len - period;
			}
			else
			{
				j += i - suffix + 1;
				memory = 0;
			}
		}
	}
	else
	{
		/* The two halves of needle are distinct; no extra memory is
		required, and any mismatch results in a maximal shift.  */
		period = MAX(suffix, needle_len - suffix) + 1;
		j = 0;
		while (AVAILABLE(haystack, haystack_len, j, needle_len))
		{
			/* Scan for matches in right half.  */
			i = suffix;
			while (i < needle_len && (CANON_ELEMENT(needle[i])
				== CANON_ELEMENT(haystack[i + j])))
				++i;
			if (needle_len <= i)
			{
				/* Scan for matches in left half.  */
				i = suffix - 1;
				while (i != SIZE_MAX && (CANON_ELEMENT(needle[i])
					== CANON_ELEMENT(haystack[i + j])))
					--i;
				if (i == SIZE_MAX)
					return (RETURN_TYPE)(haystack + j);
				j += period;
			}
			else
				j += i - suffix + 1;
		}
	}
	return NULL;
}

static RETURN_TYPE
two_way_long_needle(const unsigned char *haystack, size_t haystack_len,
	const unsigned char *needle, size_t needle_len)
{
	size_t i; /* Index into current byte of NEEDLE.  */
	size_t j; /* Index into current window of HAYSTACK.  */
	size_t period; /* The period of the right half of needle.  */
	size_t suffix; /* The index of the right half of needle.  */
	size_t shift_table[1U << CHAR_BIT]; /* See below.  */

										/* Factor the needle into two halves, such that the left half is
										smaller than the global period, and the right half is
										periodic (with a period as large as NEEDLE_LEN - suffix).  */
	suffix = critical_factorization(needle, needle_len, &period);

	/* Populate shift_table.  For each possible byte value c,
	shift_table[c] is the distance from the last occurrence of c to
	the end of NEEDLE, or NEEDLE_LEN if c is absent from the NEEDLE.
	shift_table[NEEDLE[NEEDLE_LEN - 1]] contains the only 0.  */
	for (i = 0; i < 1U << CHAR_BIT; i++)
		shift_table[i] = needle_len;
	for (i = 0; i < needle_len; i++)
		shift_table[CANON_ELEMENT(needle[i])] = needle_len - i - 1;

	/* Perform the search.  Each iteration compares the right half
	first.  */
	if (CMP_FUNC(needle, needle + period, suffix) == 0)
	{
		/* Entire needle is periodic; a mismatch can only advance by the
		period, so use memory to avoid rescanning known occurrences
		of the period.  */
		size_t memory = 0;
		size_t shift;
		j = 0;
		while (AVAILABLE(haystack, haystack_len, j, needle_len))
		{
			/* Check the last byte first; if it does not match, then
			shift to the next possible match location.  */
			shift = shift_table[CANON_ELEMENT(haystack[j + needle_len - 1])];
			if (0 < shift)
			{
				if (memory && shift < period)
				{
					/* Since needle is periodic, but the last period has
					a byte out of place, there can be no match until
					after the mismatch.  */
					shift = needle_len - period;
					memory = 0;
				}
				j += shift;
				continue;
			}
			/* Scan for matches in right half.  The last byte has
			already been matched, by virtue of the shift table.  */
			i = MAX(suffix, memory);
			while (i < needle_len - 1 && (CANON_ELEMENT(needle[i])
				== CANON_ELEMENT(haystack[i + j])))
				++i;
			if (needle_len - 1 <= i)
			{
				/* Scan for matches in left half.  */
				i = suffix - 1;
				while (memory < i + 1 && (CANON_ELEMENT(needle[i])
					== CANON_ELEMENT(haystack[i + j])))
					--i;
				if (i + 1 < memory + 1)
					return (RETURN_TYPE)(haystack + j);
				/* No match, so remember how many repetitions of period
				on the right half were scanned.  */
				j += period;
				memory = needle_len - period;
			}
			else
			{
				j += i - suffix + 1;
				memory = 0;
			}
		}
	}
	else
	{
		/* The two halves of needle are distinct; no extra memory is
		required, and any mismatch results in a maximal shift.  */
		size_t shift;
		period = MAX(suffix, needle_len - suffix) + 1;
		j = 0;
		while (AVAILABLE(haystack, haystack_len, j, needle_len))
		{
			/* Check the last byte first; if it does not match, then
			shift to the next possible match location.  */
			shift = shift_table[CANON_ELEMENT(haystack[j + needle_len - 1])];
			if (0 < shift)
			{
				j += shift;
				continue;
			}
			/* Scan for matches in right half.  The last byte has
			already been matched, by virtue of the shift table.  */
			i = suffix;
			while (i < needle_len - 1 && (CANON_ELEMENT(needle[i])
				== CANON_ELEMENT(haystack[i + j])))
				++i;
			if (needle_len - 1 <= i)
			{
				/* Scan for matches in left half.  */
				i = suffix - 1;
				while (i != SIZE_MAX && (CANON_ELEMENT(needle[i])
					== CANON_ELEMENT(haystack[i + j])))
					--i;
				if (i == SIZE_MAX)
					return (RETURN_TYPE)(haystack + j);
				j += period;
			}
			else
				j += i - suffix + 1;
		}
	}
	return NULL;
}

void *
memmem(const void *haystack_start, size_t haystack_len,
	const void *needle_start, size_t needle_len)
{
	/* Abstract memory is considered to be an array of 'unsigned char' values,
	not an array of 'char' values.  See ISO C 99 section 6.2.6.1.  */
	const unsigned char *haystack = (const unsigned char *)haystack_start;
	const unsigned char *needle = (const unsigned char *)needle_start;

	if (needle_len == 0)
		/* The first occurrence of the empty string is deemed to occur at
		the beginning of the string.  */
		return (void *)haystack;

	/* Sanity check, otherwise the loop might search through the whole
	memory.  */
	if (__builtin_expect(haystack_len < needle_len, 0))
		return NULL;

	/* Use optimizations in memchr when possible, to reduce the search
	size of haystack using a linear algorithm with a smaller
	coefficient.  However, avoid memchr for long needles, since we
	can often achieve sublinear performance.  */
	if (needle_len < LONG_NEEDLE_THRESHOLD)
	{
		haystack = memchr(haystack, *needle, haystack_len);
		if (!haystack || __builtin_expect(needle_len == 1, 0))
			return (void *)haystack;
		haystack_len -= haystack - (const unsigned char *)haystack_start;
		if (haystack_len < needle_len)
			return NULL;
		return two_way_short_needle(haystack, haystack_len, needle, needle_len);
	}
	else
		return two_way_long_needle(haystack, haystack_len, needle, needle_len);
}