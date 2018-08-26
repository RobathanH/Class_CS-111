//NAME: Robathan Harries
//EMAIL: r0b@ucla.edu
//ID: 904836501
//SLIPDAYS: 0

#include "SortedList.h"
#include <stdlib.h>
#include <sched.h>
#include <string.h>


void SortedList_insert(SortedList_t *list, SortedListElement_t *element) {

  if (list == NULL || element == NULL) return;
  
  SortedListElement_t *current = list;
  while (current->next != list && strcmp(current->next->key, element->key) < 0) current = current->next;
  //Create double link between element and current->next
  current->next->prev = element;
  element->next = current->next;

  if (opt_yield & INSERT_YIELD) sched_yield();

  //Create double link between current and element
  current->next = element;
  element->prev = current;
}

int SortedList_delete(SortedListElement_t *element) {
  //Error check
  if (element == NULL || element->next->prev != element || element->prev->next != element) return 1;

  //Reroute pointers of neighboring elements
  element->next->prev = element->prev;

  if (opt_yield & DELETE_YIELD) sched_yield();

  element->prev->next = element->next;

  return 0;
}

SortedListElement_t *SortedList_lookup(SortedList_t *list, const char *key) {

  SortedListElement_t *current = list->next;

  while (current != list) {

    if (strcmp(current->key, key) > 0) return NULL;
    else if (strcmp(current->key, key) == 0) return current;
    
    if (opt_yield & LOOKUP_YIELD) sched_yield();

    current = current->next;
  }
  
  return NULL; //Searched all the way through the list
}

int SortedList_length(SortedList_t *list) {
  SortedListElement_t *current = list->next;
  int count = 0;

  while (current != list) {
    count += 1;

    //Check pointers
    if (current->next->prev != current || current->prev->next != current) return -1;

    if (opt_yield & LOOKUP_YIELD) sched_yield();

    current = current->next;
  }
  return count;
}
