#include <stdlib.h>
#include <string.h>

#include "stratagus.h"
#include "util.h"

/*
    Mixture of hash table and binary tree.

    First level is a standard hash with the hashpjw function
    from the dragon book. But instead of a linked list in each
    slot I use a binary tree.
    To balance the tree, I take the low-byte of the full hash value
    (before the modulo) as the first char of each key.
    Storing increasing keys does not generate a perfectly balanced
    tree but one that is as good as one generated by random keys.

    usage:
	to define a hash table:
	    hashtable(data-type, table-size) identifier;
	    hashtable(data-type, table-size) id1, id2, id3;

	    data-type should be a simple type, a struct, or
	    a union. the special type hash_no_data is provided
	    when no data is needed.
	    table-size should be a prime.

	to look for an entry:
	    hash_find(table-identifier, string)

	to look for an entry and create a new one if not present:
	    hash_get(table-identifier, string)

	to add an entry
	    hash_add(table-identifier, string)

	    (this is an alias for hash_get(...))

	to get the string associated with an entry:
	    hash_name(table-identifier, hash_get/find(...))

	to get statistics about a hashtable;
	    struct hash_st st;
	    hash_stat(table-identifier, &st);
*/

struct symbol
{
	struct symbol* left;
	struct symbol* right;
	// contains user struct and name
	Uint8 misc[2];
};

static inline Uint32 hash(const Uint8* str)
{
	Uint32 h;

	h = 0;
	while (*str) {
		h = (h << 4) ^ (h >> 28) ^ *str++;
	}

	return h ? h : 1;
}

/**
**  Find a symbol. Return 0 if not found.
*/
const void* _hash_find(const Uint8* id, const void* tab, int size, int usize)
{
	const struct symbol* s;
	Uint32 h;
	int i;

	h = hash(id);
	s = ((const struct symbol**)tab)[h % size];

	while (s) {
		i = (Uint8)h - s->misc[usize];
		if (i == 0) {
			i = strcmp(id, s->misc + usize + 1);
			if (i == 0) {
				return s->misc;
			}
		}
		s = i < 0 ? s->left : s->right;
	}
	return 0;
}

/**
**  Get a symbol. Create if not found.
*/
void* _hash_get(const Uint8* id, void* tab, int size, int usize)
{
	struct symbol* s;
	struct symbol** ss;
	Uint32 h;
	int i;

	h = hash(id);
	ss = &((struct symbol**)tab)[h % size];

	while ((s = *ss)) {
		i = (Uint8)h - s->misc[usize];
		if (i == 0) {
			i = strcmp(id, s->misc + usize + 1);
			if (i == 0) {
				return s->misc;
			}
		}
		ss = i < 0 ? &s->left : &s->right;
	}

	*ss = s = malloc(sizeof(*s) + usize + strlen(id));

	s->left = 0;
	s->right = 0;
	memset(s->misc, 0, usize);
	s->misc[usize] = (Uint8)h;
	strcpy(s->misc + usize + 1, id);

	return s->misc;
}

/**
**  Delete a symbol.
*/
void _hash_del(const Uint8* id, void* tab, int size, int usize)
{
	struct symbol* s;
	struct symbol** ss;
	Uint32 h;
	int i;

	h = hash(id);
	ss = &((struct symbol**)tab)[h % size];

	while ((s = *ss)) {
		i = (Uint8)h - s->misc[usize];
		if (i == 0) {
			i = strcmp(id, s->misc + usize + 1);
			if (i == 0) {
				/* found, now remove it */
				if (s->left == 0) {
					*ss = s->right;
				} else if (s->right == 0) {
					*ss = s->left;
				} else {
					struct symbol* t;
					struct symbol** tt;

					for (tt = &s->right; (t = *tt)->left; tt = &t->left) {
					}
					*tt = t->right;
					t->left = s->left;
					t->right = s->right;
					*ss = t;
				}
				free(s);
				return;
			}
		}
		ss = i < 0 ? &s->left : &s->right;
	}
}

static void _stat(int depth, struct symbol* s, struct hash_st* st)
{
	while (s) {
		if (st->maxdepth < depth) {
			st->maxdepth = depth;
		}
		st->nelem++;
		st->middepth += depth;
		depth++;
		_stat(depth, s->left, st);
#if 0
		printf("<%s>\t", s->misc+5);
		if (s->left) {
			printf("<%s>\t", s->left->misc+5);
		} else {
			printf(".\t");
		}
		if (s->right) {
			printf("<%s>\n", s->right->misc+5);
		} else {
			printf(".\n");
		}
#endif
		s = s->right;
	}
}

void _hash_stat(void* tab, int size, struct hash_st* st)
{
	struct symbol** s;

	s = (struct symbol**)tab;

	st->nelem = 0;
	st->maxdepth = 0;
	st->middepth = 0;
	st->hashsize = size;

	while (size--) {
		_stat(1, *s++, st);
	}

	if (st->nelem) {
		st->middepth = (st->middepth * 1000) / st->nelem;
	}
}



/**
**  Standard implementation of getopt(3).
**
**  One extension: If the first character of the optionsstring is a ':'
**  the error return for 'argument required' is a ':' not a '?'.
**  This makes it easier to differentiate between an 'illegal option' and
**  an 'argument required' error.
*/

#ifdef _MSC_VER

#include <io.h>
#include <string.h>

int	opterr = 1;
int	optind = 1;
int	optopt;
char* optarg;

static void getopt_err(char* argv0, char* str, char opt)
{
	if (opterr) {
		char errbuf[2];
		char* x;

		errbuf[0] = opt;
		errbuf[1] = '\n';

		while ((x = strchr(argv0, '/'))) {
			argv0 = x + 1;
		}

		write(2, argv0, strlen(argv0));
		write(2, str, strlen(str));
		write(2, errbuf, 2);
	}
}

int getopt(int argc, char** argv, char* opts)
{
	static int sp = 1;
	register int c;
	register char *cp;

	optarg = NULL;

	if (sp == 1) {
		if (optind >= argc || argv[optind][0] != '-' || argv[optind][1] == '\0') {
			return EOF;
		} else if (!strcmp(argv[optind], "--")) {
			optind++;
			return EOF;
		}
	}
	optopt = c = argv[optind][sp];
	if (c == ':' || (cp = strchr(opts, c)) == NULL) {
		getopt_err(argv[0], ": illegal option -", (char)c);
		cp = "xx";	/* make the next if false */
		c = '?';
	}
	if (*++cp == ':') {
		if (argv[optind][++sp] != '\0') {
			optarg = &argv[optind++][sp];
		} else if (++optind < argc) {
			optarg = argv[optind++];
		} else {
			getopt_err(argv[0], ": option requires an argument -", (char)c);
			c = (*opts == ':') ? ':' : '?';
		}
		sp = 1;
	} else if (argv[optind][++sp] == '\0') {
		optind++;
		sp = 1;
	}
	return c;
}

#endif /* _MSVC */
