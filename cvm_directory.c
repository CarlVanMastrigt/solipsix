/**
Copyright 2024,2025 Carl van Mastrigt

This file is part of solipsix.

solipsix is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

solipsix is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with solipsix.  If not, see <https://www.gnu.org/licenses/>.
*/


#define _XOPEN_SOURCE 700


#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "cvm_utils.h"
#include "cvm_directory.h"
#include "sorts/quicksort.h"


// generic functions used by utilities
void cvm_directory_add_entry(struct cvm_directory* directory, char* name, uint32_t flags, uint64_t mod_t_sec, uint32_t mod_t_nsec)
{
	size_t len = strlen(name) + 1;//+1 for /0 terminator character
	assert(mod_t_nsec < 1000000000);

	if(directory->entry_count_total == directory->entry_space)
	{
		directory->entry_space *= 2;
		directory->entries = realloc(directory->entries, sizeof(struct cvm_directory_entry) * directory->entry_space);
	}
	directory->entries[directory->entry_count_total++] = (struct cvm_directory_entry)
	{
		.entry_name_offset = directory->filename_buffer_offset,
		.flags = flags,
		.mod_t_nsec = mod_t_nsec,
		.mod_t_sec = mod_t_sec,
	};

	while(directory->filename_buffer_offset + len > directory->filename_buffer_space)
	{
		directory->filename_buffer_space *= 2;
		directory->filename_buffer = realloc(directory->filename_buffer, sizeof(char) * directory->filename_buffer_space);
	}
	
	memcpy(directory->filename_buffer + directory->filename_buffer_offset, name, sizeof(char) * len);
	directory->filename_buffer_offset += len;
}

///file directory should map indices (0-n in display) to relevant lookup information (type and index in dir) 
void cvm_directory_ensure_path_space(struct cvm_directory* directory, size_t required_space)
{
	if(required_space > directory->path_space)
    {
    	do directory->path_space *= 2;
    	while(required_space > directory->path_space);
    	directory->path = realloc(directory->path, sizeof(char) * directory->path_space);
    }
}








// `util` functions should be implemented in platform specific manner
// bool cvm_directory_utility_set_path(struct cvm_directory* directory, const char* path);
// void cvm_directory_utility_scan(struct cvm_directory* directory);
// void cvm_directory_utility_goto_parent(struct cvm_directory* directory);
// void cvm_directory_utility_get_path(struct cvm_directory* directory, const char* to_append, size_t* required_space, char* path_buffer);


#define CVM_DIRECTORY_UTILITY_IMPLEMENTATION
#include "cvm_directory_utility_implementation.c"
#undef CVM_DIRECTORY_UTILITY_IMPLEMENTATION





/// used as wrapper over function pointer
static inline bool cvm_directory_sort_function(const struct cvm_directory_entry* entry_1, const struct cvm_directory_entry* entry_2, const struct cvm_directory* directory)
{
	return directory->sort_function(entry_1, entry_2, directory);
}

#define SOL_COMPARE_LT cvm_directory_sort_function
SOL_QUICKSORT_WITH_CONTEXT(struct cvm_directory_entry, cvm_directory_sort, 8, static, const struct cvm_directory*);
#undef SOL_COMPARE_LT



static inline void cvm_directory_filter_and_sort(struct cvm_directory* directory)
{
	#warning need to apply filter here then sort only unfiltered range

	size_t processed_count, passing_count;
	struct cvm_directory_entry tmp;
	struct cvm_directory_entry* entries;
	uint32_t exclude_flags;

	entries = directory->entries;
	exclude_flags = directory->filter_exclude_flags;

	processed_count = 0;
	passing_count = directory->entry_count_total;

	while(processed_count < passing_count)
	{
		if(entries[processed_count].flags & directory->filter_exclude_flags)
		{
			passing_count--;
			tmp = entries[processed_count];
			entries[processed_count] = entries[passing_count];
			entries[passing_count] = tmp;
		}
		else
		{
			processed_count++;
		}
	}

	assert(processed_count==passing_count);
	directory->entry_count_visible = passing_count;

	assert(directory->sort_function);

	cvm_directory_sort(entries, passing_count, directory);
}

#warning move this to utilities and make it more parsable (and fucking validate/understand it)
static inline int cvm_strcmp_numbers(const char * s1,const char * s2)
{
	// in effect returns sign(s1 - s2) in terms of position in ordered list
    char c1,c2,l1,l2,d;
    const char *n1,*n2,*e1,*e2;

    do
    {
        c1=*s1;
        c2=*s2;

        if(c1>='0' && c1<='9' && c2>='0' && c2<='9')///enter number analysis scheme
        {
            ///already know first is digit
            n1=s1+1;
            n2=s2+1;
            ///find end of number block
            while(*n1>='0' && *n1<='9')n1++;
            while(*n2>='0' && *n2<='9')n2++;
            e1=n1;
            e2=n2;
            d=0;///initially treat numbers as the same, let most significat different digit determine their difference
            while(1)
            {
                ///start with step back, last char, by definition, isnt a number
                n1--;
                n2--;
                if(n1<s1)
                {
                    // if(!d && n2>=s2)return -1;// more digits go later if "number" is the same
                    while(n2>=s2) if(*n2-- != '0') return -1;///n2 is larger number
                    if(d) return d;
                    else break;
                }
                if(n2<s2)
                {
                    // if(!d && n1>=s1) return 1; // more digits go later if "number" is the same
                    while(n1>=s1) if(*n1-- != '0') return 1;///n1 is larger number
                    if(d) return d;
                    else break;
                }
                if(*n1!=*n2)
                {
                	// most significant different digit
                	d=*n1-*n2;
                }
            }

            ///skip to the point past the number block (reset s and c)
            s1=e1;
            s2=e2;
            c1=*s1;
            c2=*s2;
        }
        else
        {
            s1++;
            s2++;
        }


        #warning is there an ordering for non-latin alphabets that can easily be respected??
        // give alphabetical order higher priority than capitalization
        l1=c1;
        l2=c2;
        if(l1>='A' && l1<='Z')l1-='A'-'a';
        if(l2>='A' && l2<='Z')l2-='A'-'a';
        if(l1!=l2)
        {
        	return l1-l2;
        }
    }
    while((c1==c2)&&(c1)&&(c2));

    return (c1-c2);
}

static bool cvm_directory_sort_entries_name_ascending(const struct cvm_directory_entry* entry_1, const struct cvm_directory_entry* entry_2, const struct cvm_directory* directory)
{
	const char* str_1 = directory->filename_buffer + entry_1->entry_name_offset;
	const char* str_2 = directory->filename_buffer + entry_2->entry_name_offset;

	///directory goes first if they're of different types (dir vs non dir)
	if((entry_1->flags ^ entry_2->flags) & CVM_DIRECTORY_FLAG_DIRECTORY)
	{
		return entry_1->flags & CVM_DIRECTORY_FLAG_DIRECTORY;
	}

	return cvm_strcmp_numbers(str_1,str_2) < 0;
}

static bool cvm_directory_sort_entries_name_descending(const struct cvm_directory_entry* entry_1, const struct cvm_directory_entry* entry_2, const struct cvm_directory* directory)
{
	const char* str_1 = directory->filename_buffer + entry_1->entry_name_offset;
	const char* str_2 = directory->filename_buffer + entry_2->entry_name_offset;

	///directory goes first if they're of different types (dir vs non dir)
	if((entry_1->flags ^ entry_2->flags) & CVM_DIRECTORY_FLAG_DIRECTORY)
	{
		return entry_1->flags & CVM_DIRECTORY_FLAG_DIRECTORY;
	}

	return cvm_strcmp_numbers(str_1,str_2) > 0;
}

static bool cvm_directory_sort_entries_time_ascending(const struct cvm_directory_entry* entry_1, const struct cvm_directory_entry* entry_2, const struct cvm_directory* directory)
{
	///directory goes first if they're of different types (dir vs non dir)
	if((entry_1->flags ^ entry_2->flags) & CVM_DIRECTORY_FLAG_DIRECTORY)
	{
		return entry_1->flags & CVM_DIRECTORY_FLAG_DIRECTORY;
	}

	if(entry_1->mod_t_sec == entry_2->mod_t_sec)
	{
		// if seconds the same use nanoseconds
		return entry_1->mod_t_nsec < entry_2->mod_t_nsec;
	}
	else
	{
		return entry_1->mod_t_sec < entry_2->mod_t_sec;
	}
}

static bool cvm_directory_sort_entries_time_descending(const struct cvm_directory_entry* entry_1, const struct cvm_directory_entry* entry_2, const struct cvm_directory* directory)
{
	///directory goes first if they're of different types (dir vs non dir)
	if((entry_1->flags ^ entry_2->flags) & CVM_DIRECTORY_FLAG_DIRECTORY)
	{
		return entry_1->flags & CVM_DIRECTORY_FLAG_DIRECTORY;
	}

	if(entry_1->mod_t_sec == entry_2->mod_t_sec)
	{
		// if seconds the same use nanoseconds
		return entry_1->mod_t_nsec > entry_2->mod_t_nsec;
	}
	else
	{
		return entry_1->mod_t_sec > entry_2->mod_t_sec;
	}
}





void cvm_directory_initialise(struct cvm_directory* directory, const char* initial_path, const char* fallback_path)
{
	size_t initial_path_length;

	*directory = (struct cvm_directory)
	{
		.fallback_path = cvm_strdup(fallback_path),
		.path = malloc(sizeof(char)*64),
		.path_space = 64,

		.entries = malloc(sizeof(struct cvm_directory_entry)*16),
		.entry_space = 16,
		.entry_count_total = 0,
		.entry_count_visible = 0,

		.filter_exclude_flags = 0,
		.filter_extension_buffer = malloc(sizeof(char)*64),
		.filter_extension_buffer_space = 64,

		.filename_buffer = malloc(sizeof(char)*4096),
		.filename_buffer_space = 4096,
		.filename_buffer_offset = 0,

		.sort_function = &cvm_directory_sort_entries_name_ascending,
	};

	cvm_directory_utility_set_path(directory, initial_path);
	cvm_directory_utility_scan(directory);
    cvm_directory_filter_and_sort(directory);
}

void cvm_directory_terminate(struct cvm_directory* directory)
{
	free(directory->fallback_path);
	free(directory->path);
	free(directory->entries);
	free(directory->filter_extension_buffer);
	free(directory->filename_buffer);
}

void cvm_directory_goto_parent(struct cvm_directory* directory)
{
	cvm_directory_utility_goto_parent(directory);
	cvm_directory_utility_scan(directory);
	cvm_directory_filter_and_sort(directory);
}

void cvm_directory_get_entry_path(struct cvm_directory* directory, uint32_t entry_index, size_t* required_space, char* path_buffer)
{
	char* entry_name;

	assert(entry_index < directory->entry_count_visible);

	entry_name = directory->filename_buffer + directory->entries[entry_index].entry_name_offset;

	cvm_directory_utility_get_path(directory, entry_name, required_space, path_buffer);
}

void cvm_directory_get_file_path(struct cvm_directory* directory, const char* path_relative_filename, size_t* required_space, char* path_buffer)
{
	cvm_directory_utility_get_path(directory, path_relative_filename, required_space, path_buffer);
}

void cvm_directory_reload(struct cvm_directory* directory)
{
	cvm_directory_utility_scan(directory);
	cvm_directory_filter_and_sort(directory);
}

void cvm_directory_set_ordering_name(struct cvm_directory* directory, bool reversed)
{
	if(reversed)
	{
		cvm_directory_set_ordering(directory, &cvm_directory_sort_entries_name_descending);
	}
	else
	{
		cvm_directory_set_ordering(directory, &cvm_directory_sort_entries_name_ascending);
	}
}

void cvm_directory_set_ordering_time(struct cvm_directory* directory, bool reversed)
{
	if(reversed)
	{
		cvm_directory_set_ordering(directory, &cvm_directory_sort_entries_time_descending);
	}
	else
	{
		cvm_directory_set_ordering(directory, &cvm_directory_sort_entries_time_ascending);
	}
}

void cvm_directory_set_ordering(struct cvm_directory* directory, bool (*sort_function)(const struct cvm_directory_entry*, const struct cvm_directory_entry*, const struct cvm_directory*))
{
	if(sort_function != directory->sort_function)
	{
		directory->sort_function = sort_function;
		cvm_directory_filter_and_sort(directory);
	}
}


// for widgets that reference this we should avoid having anything built on top of it, this becomes complicated in the case of text bar though (for selections)
// this can be done in custom fashion though (validating contents and querying basis before invalidating local contents)
// it DOES however mean we should move all sort/filter operations to the directory

// all widgets accessing directory should use opaque accesses for all variables!, this is quite difficult (but not impossible) for textbox displaying selected widget

void cvm_directory_debug_print(struct cvm_directory* directory)
{
	size_t i;
	printf("directory: %s\n", directory->path);
	puts("==== visible entries ====");
	for(i=0; i<directory->entry_count_visible; i++)
	{
		printf("\"%s\" (0x%X)\n",directory->filename_buffer+directory->entries[i].entry_name_offset, directory->entries[i].flags);
	}
	puts("==== non-visible entries ====");
	for(i=directory->entry_count_visible; i<directory->entry_count_total; i++)
	{
		printf("\"%s\" (0x%X)\n",directory->filename_buffer+directory->entries[i].entry_name_offset, directory->entries[i].flags);
	}
	puts("\n");
}

