/*
 * FCRON - periodic command scheduler
 *
 *  Copyright 2000-2009 Thibault Godouet <fcron@free.fr>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *  The GNU General Public License can also be found in the file
 *  `LICENSE' that comes with the fcron source distribution.
 */

 /* $Id: temp_file.c,v 1.6 2007/04/14 18:04:19 thib Exp thib $ */

/* List of jobs currently being executed.
 * This is a wrapper for an u_list (unordered list) (see u_list.h and u_list.c),
 * to make the rest of the code clearer and as a way to ensure the compiler can checks
 * the type in the rest of the code (u_list extensively uses the void type) */

#include "global.h"
#include "fcron.h"
#include "env_list.h"

env_list_t *env_list_init(void)
{
    return (env_list_t *) u_list_init(sizeof(env_t), 20, 10);
}

env_list_t *
env_list_copy(env_list_t *list)
{
    env_list_t *new_list = NULL;
    env_t *e = NULL;

    /* copy the list structure */
    new_list = (env_list_t *) u_list_copy(u_list_t *list);

    /* for now the new list points to the same data strings - duplicate them */
    for ( e = env_list_first(new_list) ; e != NULL ; e = env_list_next(e) ) {
        e->e_name = strdup2(e->e_name);
        e->e_value = strdup2(e->e_value);
    }

    return new_list;

}

env_t *
env_list_setenv(env_list_t *list, char *name, char *value, int overwrite)
{
    env_t e = { NULL, NULL };
    env_t *c = NULL;

    /* check if a var 'name' already exists */
    for ( c = env_list_first(list) ; c != NULL ; c = env_list_next(c) ) {
        if ( strcmp(name, c->e_name) == 0 ) {
            /* variable already set: overwrite the value if asked
	     * and return that entry */
            if ( overwrite == 1 ) {
                free_safe(c->e_value);
                c->e_value = strdup2(value);
                env_list_end_iteration(list);
            }
            return c;
	}
    }

    /* if we're here we didn't find a var called 'name': add it */
    e.e_name = strdup2(name);
    e.e_value = strdup2(value);
    return (env_t *) u_list_add( (u_list_t *) list, (u_list_entry_t *) &e);
}

env_t *
env_list_add(env_list_t *list, env_t *e)
{
    return (env_t *) u_list_add( (u_list_t *) list, (u_list_entry_t *) &e);
}

env_t *
env_list_first(env_list_t *list)
{
    return (env_t *) u_list_first((u_list_t *) list);
}

env_t *
env_list_next(env_list_t *list)
{
    return (env_t *) u_list_next((u_list_t *) list);
}

void
env_list_end_iteration(env_list_t *list)
{
    u_list_end_iteration((u_list_t *) list);
}

env_list_t *
env_list_destroy(env_list_t *list)
    /* free() the memory allocated for list and returns NULL */
{
    env_t *c = NULL;

    /* make sure the iteration below won't fail in case one was already iterating */
    env_list_end_iteration(list);
    /* free the data in the env_t entries */
    for ( c = env_list_first(list) ; c != NULL ; c = env_list_next(c) ) {
        free_safe(c->e_name);
        free_safe(c->e_value);
    }
    /* free the actual list structure */
    return (env_list_t *) u_list_destroy((u_list_t *) list);
}

char **
env_list_export_envp(env_list_t *list)
/* export list data as a char **envp structure to use as an argument of execle() */
{
    env_t *c = NULL;
    int i = 0;
    char **envp = NULL;
    size_t len = 0;

    envp = calloc(l->num_entries + 1); /* +1 for the end-of-list NULL */
    if ( envp == NULL )
	    die_e("Could not allocate memory for the environment");

    for ( c=env_list_first(list), i=0 ; c != NULL && i < l->num_entries ; c=env_list_next(c), i++ ) {
        len = strlen(c->e_name) + strlen(c->e_value) + 2; /* +1 for =, +1 for \0 */
        envp[i] = calloc(1, len);
	if ( envp[i] == NULL )
            die_e("Could not allocaed memory for an environment entry");
	snprintf(envp[i], len, "%s=%s", c->e_name, c->e_value);
    }
    /* add a NULL as a end-of-list marker */
    envp[ (l->num_entries + 1 ) - 1] = NULL;

    return envp;
}

void
env_list_free_envp(char **envp)
{
    char *p = NULL;

    for ( p = envp[0] ; p != NULL ; p++ ) {
        free_safe(p);
    }
    free_safe(envp);

}

