/*
 * Copyright 2006 Martin Hedenfalk <martin@bzero.se>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "MHSysTree.h"

@implementation MHSysTree

RB_GENERATE(id_tree, rb_entry, entry, id_cmp);

int id_cmp(struct rb_entry *a, struct rb_entry *b)
{
    return (int)[a->obj performSelector:@selector(compare:) withObject:b->obj];
}

- (id)init
{
    if ((self = [super init])) {
        RB_INIT(&root);
        nitems = 0;
    }
    
    return self;
}

- (void)dealloc
{
    [self removeAllObjects];
    [super dealloc];
}

- (void)addObject:(id)anObject
{
    struct rb_entry *e = malloc(sizeof(struct rb_entry));
    e->obj = [anObject retain];
    RB_INSERT(id_tree, &root, e);
    nitems++;
}

- (void)addObjectsFromArray:(NSArray *)anArray
{
    NSEnumerator *e = [anArray objectEnumerator];
    id obj;
    while ((obj = [e nextObject]) != nil)
        [self addObject:obj];
}

- (void)removeEntry:(struct rb_entry *)anEntry
{
    RB_REMOVE(id_tree, &root, anEntry);
    [anEntry->obj release];
    free(anEntry);
    nitems--;
}

- (void)removeObject:(id)anObject
{
    struct rb_entry *e = [self lookup:anObject];
    if (e)
        [self removeEntry:e];
}

- (unsigned)count
{
    return nitems;
}

- (BOOL)containsObject:(id)anObject
{
    return [self lookup:anObject] == nil ? NO : YES;
}

- (id)find:(id)anObject
{
    struct rb_entry *e = [self lookup:anObject];
    
    return e ? e->obj : nil;
}

- (struct rb_entry *)lookup:(id)anObject
{
    struct rb_entry find;
    find.obj = anObject;
    
    return RB_FIND(id_tree, &root, &find);
}

- (NSArray *)allObjects
{
    NSMutableArray *array = [NSMutableArray arrayWithCapacity:nitems];
    struct rb_entry *e;
    RB_FOREACH(e, id_tree, &root) {
        [array addObject:e->obj];
    }

    return array;
}

- (void)removeAllObjects
{
    struct rb_entry *e, *next;
    for (e = RB_MIN(id_tree, &root); e; e = next) {
        next = RB_NEXT(id_tree, &root, e);
        [self removeEntry:e];
    }
}

@end

/* vim: ft=objc
 */
