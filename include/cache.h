#ifndef CACHE_H
#define CACHE_H

void save_to_cache(int g_hover, int e_hover, int true_hover, char *cfg_name);

void load_cache(int *g_hover, int *e_hover, int *true_hover, char *new_cfg_name);

#endif
