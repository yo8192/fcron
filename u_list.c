/*
 * FCRON - periodic command scheduler 
 *
 *  Copyright 2000-2008 Thibault Godouet <fcron@free.fr>
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

/*
 * Unordered list of generic items
 */

#include "global.h"
#include "fcron.h"
#include "u_list.h"

/* private functions: */
int u_list_resize_array(u_list_t *l);
u_list_entry_t *u_list_last(u_list_t *l);

u_list_t *
u_list_init(size_t entry_size, int init_size, int grow_size) 
/* Create a new unordered list
 * Returns the newly created unorderd list
 * Enough memory to hold init_size entries will initially be allocated,
 * and it will grow by grow_size entries when more space is needed.
 * Dies on error. */
{
    u_list_t *l = NULL;

    /* sanity check */
    if ( entry_size < 1 || init_size < 1 || grow_size < 1 )
	die("Invalid arguments for u_list_init(): entry_size=%d, init_size=%d, "
	    "grow_size=%d", entry_size, init_size, grow_size);

    /* Allocate the list structure: */
    l = calloc(1, sizeof(struct u_list_t));
    if ( l == NULL )
	die_e("Failed creating a new unordered list: could not calloc() u_list_t "
	      "(entry_size: %d)", entry_size);

    /* Initialize the structure and allocate the array: */
    l->array_size = init_size;
    l->entry_size = entry_size;
    l->grow_size = grow_size;
    l->entries_array = calloc(init_size, entry_size);
    if ( l->entries_array == NULL )
	die_e("Failed creating a new unordered list: could not calloc array"
	      "(entry_size: %d, init_size: %d)", entry_size, init_size);

    return l;
}

int
u_list_resize_array(u_list_t *l)
/* Resize l's entries_array up to l->max_entries
 * Returns OK on success, ERR if the array is already at maximum size */
{
    u_list_entry_t *e = NULL;
    int old_size = l->array_size;

    /* sanity check */
    if ( l == NULL )
	die("Invalid argument for u_list_resize_array(): list=%d", l);
    if ( l->max_entries > 0 && l->array_size >= l->max_entries ) {
	debug("Resizing u_list_t failed because it is already at max size (size: %d)",
	      l->array_size);
	return ERR;
    }

    l->array_size = (l->array_size + l->grow_size);
    if ( l->max_entries > 0 && l->array_size > l->max_entries )
	l->array_size = l->max_entries;

    debug("Resizing u_list_t (old size: %d, new size: %d)...", old_size, l->array_size);
	
    if ( (e = calloc(l->array_size, l->entry_size)) == NULL )
	die_e("Could not calloc u_list_t to grow entries_array "
	      "(old size: %d, new size: %d)...", old_size, l->array_size);

    memcpy(e, l->entries_array, (l->entry_size * old_size));
    free_safe(l->entries_array);
    l->entries_array = e;    

    return OK;
}

u_list_entry_t *
u_list_last(u_list_t *l)
/* Returns the pointer of the last entry in the list, or NULL if l is empty */
{
    if ( l->num_entries <= 0 )
	return NULL;
    else
	return (u_list_entry_t *) 
	( (char *)l->entries_array + l->entry_size * ( l->num_entries - 1 ) );
}

u_list_entry_t * 
u_list_add(u_list_t *l, u_list_entry_t *e)
/* Add one entry to the list
 * Returns a pointer to the added element, or NULL if list is already at max size */
{
    u_list_entry_t *new = NULL;

    /* sanity check */
    if ( l == NULL || e == NULL )
	die("Invalid arguments for u_list_add(): list=%d, entry=%d", l, e);

    /* Check there is some space left, or resize the array */
    if ( l->num_entries >= l->array_size ) {
	/* no more space: attempt to grow (the following function dies on error: */
	if ( u_list_resize_array(l) != OK )
	    return NULL;
    }

    l->num_entries++;
    new = u_list_last(l);
    memcpy(new, e, l->entry_size);

    return new;
}

u_list_entry_t * 
u_list_first(u_list_t *l)
/* Return the first entry of the list (then u_list_next() can be used) */
{
    /* sanity check */
    if ( l == NULL )
	die("Invalid argument for u_list_first(): list=%d", l);

    return (l->num_entries > 0) ? l->entries_array : NULL;
}

u_list_entry_t * 
u_list_next(u_list_t *l, u_list_entry_t *e)
/* Return the entry after e */
{
    /* sanity checks */
    if (l==NULL||e==NULL)
	die("Invalid arguments for u_list_next(): list=%d, entry=%d", l, e);
    if (e < l->entries_array || e > u_list_last(l) )
	die("u_list_next(): entry out of list! (entries_array: %d, entry: %d,"
	    "num_entries: %d)", l->entries_array, e, l->num_entries);

    /* // */
    /* Undefined? -- convert soustraction to float and check if result is an int ? */
/*    if ( ( (char *) e - (char *) l->entries_array ) % l->entry_size != 0 )
	die("u_list_next(): entry shifted! (entries_array: %d, entry_size: %d, "
	    "entry: %d", l->entries_array, l->entry_size, e);
*/
    if ( e < u_list_last(l) )
	return (u_list_entry_t *) ( (char *) e + l->entry_size);
    else 
	return NULL;
}

void
u_list_remove(u_list_t *l, u_list_entry_t *e)
{
    u_list_entry_t *last = NULL;

    /* sanity checks */
    if ( l == NULL || e == NULL )
	die("Invalid arguments for u_list_remove(): list=%d, entry=%d", l, e);
    if (e < l->entries_array || e > u_list_last(l) )
	die("u_list_next(): entry out of list! (entries_array: %d, entry: %d,"
	    "num_entries: %d)", l->entries_array, e, l->num_entries);

    last = u_list_last(l);
    if ( e < last ) {
	/* Override e with the last entry */
	memcpy(e, last, l->entry_size);
    }
    /* erase the last entry and update the number of entries */
    memset(last, 0, l->entry_size);
    l->num_entries--;
    

}

u_list_t *
u_list_destroy(u_list_t *list)
    /* free() the memory allocated for list and returns NULL */
{
    if ( list == NULL )
	die("Invalid argument for u_list_destroy(): list=%d", list);

    free_safe(list->entries_array);
    free_safe(list);
    return NULL;
}
