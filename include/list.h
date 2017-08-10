#ifndef __LIST_H__
#define __LIST_H__

# define POISON_POINTER_DELTA 0xdead000000000000

#define LIST_POISON1  ((void *)0x100 + POISON_POINTER_DELTA)
#define LIST_POISON2  ((void *)0x200 + POISON_POINTER_DELTA)

#define container_of(ptr, type, member) ({                      \
            const typeof(((type *)0)->member) *__mptr = (ptr);  \
            (type *)((char *)__mptr - offsetof(type,member));   \
            })

#define rcu_assign_pointer(p, v)    \
    do {                            \
        __sync_synchronize();       \
        (p) = (v);                  \
    } while (0)

#define ACCESS_ONCE(x) (*(volatile typeof(x) *)&(x))

//smp_read_barrier_depends(); this is not used ?
//instead by __sync_synchronize?
#define rcu_dereference(p)                                  \
    ({                                                      \
     typeof(*p) *_________p1 = (typeof(*p)*)ACCESS_ONCE(p); \
     __sync_synchronize();                                  \
     ((typeof(*p) *)(_________p1));                         \
     })

struct list_head {
    struct list_head *next, *prev;
};

#define list_next_rcu(list)	(*((struct list_head **)(&(list)->next)))

static inline void
__list_add_rcu(struct list_head *new,
               struct list_head *prev, struct list_head *next)
{
    new->next = next;
    new->prev = prev;
    rcu_assign_pointer(list_next_rcu(prev), new);
    next->prev = new;
}

static inline void
list_add_rcu(struct list_head *new, struct list_head *head)
{
    __list_add_rcu(new, head, head->next);
}

static inline void
list_add_tail_rcu(struct list_head *new, struct list_head *head)
{
    __list_add_rcu(new, head->prev, head);
}

static inline void
__list_del(struct list_head * prev, struct list_head * next)
{
    next->prev = prev;
    prev->next = next;
}

static inline void
__list_del_entry(struct list_head *entry)
{
    __list_del(entry->prev, entry->next);
}

static inline void
list_del_rcu(struct list_head *entry)
{
    __list_del_entry(entry);
    entry->prev = LIST_POISON2;
}

#define list_entry_rcu (ptr, type, member) \
    ({typeof (*ptr) *__ptr = (typeof (*ptr) *)ptr; \
     container_of((typeof(ptr))rcu_dereference(__ptr), type, member); \
     })

#define list_for_each_entry_rcu (pos, head, member) \
    for (pos = list_entry_rcu ((head)->next, typeof(*pos), member); \
                &pos->member != (head); \
                pos = list_entry_rcu (pos->member.next, typeof(*pos), member))

#define list_for_each_entry_continue_rcu (pos, head, member) 		\
    for (pos = list_entry_rcu (pos->member.next, typeof(*pos), member); \
                &pos->member != (head);	\
                pos = list_entry_rcu (pos->member.next, typeof(*pos), member))

struct hlist_head {
    struct hlist_node *first;
};

struct hlist_node {
    struct hlist_node *next, **pprev;
};

#define INIT_HLIST_HEAD(ptr) ((ptr)->first = NULL)

static inline bool
is_hlist_node_free(struct hlist_node *n)
{
    return n->pprev == LIST_POISON2;
}

static inline void
__hlist_del(struct hlist_node *n)
{
    struct hlist_node *next = n->next;
    struct hlist_node **pprev = n->pprev;
    *pprev = next;
    if (next) {
        next->pprev = pprev;
    }
}

static inline void
hlist_del_rcu(struct hlist_node *n)
{
    __hlist_del(n);
    n->pprev = LIST_POISON2;
}

#define hlist_first_rcu(head)	(*((struct hlist_node **)(&(head)->first)))
#define hlist_next_rcu(node)	(*((struct hlist_node **)(&(node)->next)))
#define hlist_pprev_rcu(node)	(*((struct hlist_node **)((node)->pprev)))

static inline void
hlist_add_head_rcu(struct hlist_node *n, struct hlist_head *h)
{
    struct hlist_node *first = h->first;

    n->next = first;
    n->pprev = &h->first;
    rcu_assign_pointer(hlist_first_rcu(h), n);
    if (first) {
        first->pprev = &n->next;
    }
}

static inline void
hlist_add_before_rcu(struct hlist_node *n, struct hlist_node *next)
{
    n->pprev = next->pprev;
    n->next = next;
    rcu_assign_pointer(hlist_pprev_rcu(n), n);
    next->pprev = &n->next;
}

static inline void
hlist_add_after_rcu(struct hlist_node *prev, struct hlist_node *n)
{
    n->next = prev->next;
    n->pprev = &prev->next;
    rcu_assign_pointer(hlist_next_rcu(prev), n);
    if (n->next) {
        n->next->pprev = &n->next;
    }
}

#define hlist_entry(ptr, type, member) container_of(ptr,type,member)

#define hlist_entry_safe(ptr, type, member) \
    ({ typeof(ptr) ____ptr = (ptr); \
     ____ptr ? hlist_entry(____ptr, type, member) : NULL; \
     })

#define __hlist_for_each_rcu(pos, head)				\
    for (pos = rcu_dereference(hlist_first_rcu(head));	\
                pos;						\
                pos = rcu_dereference(hlist_next_rcu(pos)))

#define hlist_for_each_entry_rcu(pos, head, member)			\
    for (pos = hlist_entry_safe (rcu_dereference(hlist_first_rcu(head)),\
                    typeof(*(pos)), member);			\
                pos;							\
                pos = hlist_entry_safe (rcu_dereference(hlist_next_rcu(\
                            &(pos)->member)), typeof(*(pos)), member))

#define hlist_for_each_entry_continue_rcu(pos, member)			\
    for (pos = hlist_entry_safe (rcu_dereference((pos)->member.next),\
                    typeof(*(pos)), member);			\
                pos;							\
                pos = hlist_entry_safe (rcu_dereference((pos)->member.next),\
                    typeof(*(pos)), member))

#endif
