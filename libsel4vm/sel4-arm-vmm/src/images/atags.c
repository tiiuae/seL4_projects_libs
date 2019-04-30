/*
 * Copyright 2017, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(DATA61_BSD)
 */
#include <sel4arm-vmm/atags.h>

#include <stdlib.h>
#include <assert.h>
#include <string.h>

#define HDR_SIZE(x)   (sizeof(*(x))/sizeof(uint32_t))
#define CMDLINE_SEPERATOR ", "

struct atag_core {
    struct atag_hdr hdr;
    uint32_t flags;
    uint32_t pagesize;
    uint32_t rootdev;
};

struct atag_mem {
    struct atag_hdr hdr;
    uint32_t size;
    uint32_t start;
};

struct atag_cmdline {
    struct atag_hdr hdr;
    char cmdline[];
};


static struct atag_list *atag_search(struct atag_list *head, uint32_t tag)
{
    while (head != NULL && head->hdr->tag != tag) {
        head = head->next;
    }
    return head;
}

static struct atag_list *atag_new(int size)
{
    struct atag_list *t;
    t = (struct atag_list *)malloc(sizeof(*t));
    if (t == NULL) {
        return NULL;
    }
    t->hdr = (struct atag_hdr *)malloc(size);
    if (t->hdr == NULL) {
        free(t);
        return NULL;
    }
    t->hdr->size = 0;
    t->hdr->tag = 0;
    t->next = NULL;
    return t;
}

static void atag_append(struct atag_list *atags, struct atag_list *tag)
{
    assert(atags);
    while (atags->next != NULL) {
        atags = atags->next;
    }
    atags->next = tag;
}

struct atag_list *
atags_new(void)
{
    struct atag_list *atag;
    struct atag_core *core;

    /* Create an atag */
    atag = atag_new(sizeof(*core));
    if (atag == NULL) {
        return NULL;
    }

    /* Fill in the data */
    core = (struct atag_core *)atag->hdr;
    core->hdr.size = 2 /* No structure */;
    core->hdr.tag = ATAG_CORE;
    return atag;
}

int atags_add_core(struct atag_list *atags, uint32_t flags, uint32_t pagesize, uint32_t rootdev)
{
    struct atag_core *core;
    assert(atags);

    core = (struct atag_core *)atags->hdr;
    assert(core != NULL);
    assert(core->hdr.tag == ATAG_CORE);
    /* Fill in the data */
    core->hdr.size = HDR_SIZE(core);
    core->flags = flags;
    core->pagesize = pagesize;
    core->rootdev = rootdev;
    return 0;
}

int atags_add_mem(struct atag_list *atags, uint32_t size, uint32_t start)
{
    struct atag_list *atag;
    struct atag_mem *mem;
    assert(atags);

    atag = atag_new(sizeof(*mem));
    if (atag == NULL) {
        return -1;
    }
    mem = (struct atag_mem *)atag->hdr;

    mem->hdr.size = HDR_SIZE(mem);
    mem->hdr.tag = ATAG_MEM;
    mem->size = size;
    mem->start = start;

    atag_append(atags, atag);
    return 0;
}

int atags_append_cmdline(struct atag_list *atags, const char *arg)
{
    struct atag_list *atag;
    struct atag_cmdline *cmdline, *cmdline_old;
    char *buf;
    int size;
    assert(atags);

    size = strlen(arg) + 1;
    /* Pull in the old command line atag */
    atag = atag_search(atags, ATAG_CMDLINE);
    if (atag == NULL) {
        /* allocate a new tag */
        atag = atag_new(sizeof(*cmdline) + ((size + 3) & ~0x3));
        if (atag == NULL) {
            return -1;
        }
        atag_append(atags, atag);
        cmdline = (struct atag_cmdline *)atag->hdr;
        buf = cmdline->cmdline;
    } else {
        char *old_buf;
        /* Find the extended size */
        cmdline_old = (struct atag_cmdline *)atag->hdr;
        old_buf = cmdline_old->cmdline;
        size += strlen(old_buf) + strlen(CMDLINE_SEPERATOR);
        /* allocate some memory */
        cmdline = (struct atag_cmdline *)malloc(sizeof(*cmdline) + ((size + 3) & ~0x3));
        assert(cmdline);
        if (cmdline == NULL) {
            return -1;
        }
        atag->hdr = (struct atag_hdr *)cmdline;
        /* Copy in the old buffer */
        buf = cmdline->cmdline;
        strcpy(buf, old_buf);
        buf += strlen(old_buf);
        /* Copy in the seperator */
        strcpy(buf, CMDLINE_SEPERATOR);
        buf += strlen(CMDLINE_SEPERATOR);
        /* free the old cmdline */
        free(cmdline);
    }
    strcpy(buf, arg);
    buf += strlen(arg);
    *buf = '\0';
    cmdline->hdr.tag = ATAG_CMDLINE;
    cmdline->hdr.size = (sizeof(*cmdline) + (size + 3)) / 4;
    return 0;
}

int atags_size_bytes(struct atag_list *atag)
{
    return atag->hdr->size * sizeof(uint32_t);
}

