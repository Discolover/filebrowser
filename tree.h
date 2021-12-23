struct Tree;

struct Tree *tree_new(int (*cmp)(void *key1, void *key2));
void tree_insert(struct Tree *t, void *key);
int tree_exists(struct Tree *t, void *key);
void tree_delete(struct Tree *t, void *key);
void tree_foreach(struct Tree *t, void (*fnc)(void *key));
void tree_foreach_free(struct Tree *t, void (*fnc)(void *key));
void tree_free(struct Tree *t);
