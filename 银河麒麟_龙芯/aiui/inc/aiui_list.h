#ifndef __AIUI_LIST_H__
#define __AIUI_LIST_H__

typedef struct aiui_list_s
{
    aiui_list_s *head;
    aiui_list_s *tail;
    aiui_list_s *next;
    void *val;
    int len;
} aiui_list_t;

aiui_list_t *aiui_list_create();
void aiui_list_push(aiui_list_t *head, void *val, int len);
aiui_list_t *aiui_list_pop(aiui_list_t *head);
void aiui_list_node_destroy(aiui_list_t *node);
void aiui_list_destroy(aiui_list_t *head);
#endif    // __AIUI_LIST_H__