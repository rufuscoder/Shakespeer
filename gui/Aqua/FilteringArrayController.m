
#import <Foundation/NSKeyValueObserving.h>
#import "FilteringArrayController.h"
#import "NSStringExtensions.h"

@interface FilteringArrayController (Private)
// check if the string matches all the terms in the array. (logical AND-search)
+ (BOOL)searchStrings:(NSArray *)searchStrings matchesTerms:(NSArray *)terms;
@end


@implementation FilteringArrayController


- (void)search:(id)sender
{
    [self setSearchString:[sender stringValue]];
    [self rearrangeObjects];    
}

- (NSArray *)arrangeObjects:(NSArray *)objects
{
    if (!searchString || [[searchString stringWithoutWhitespace] length] == 0 ||
       !searchKeys || [searchKeys count] == 0) {
        return [super arrangeObjects:objects];   
    }
    
    /*
     * Create array of objects that match search string. Also add any
     * newly-created object unconditionally: (a) You'll get an error if a
     * newly-added object isn't added to arrangedObjects.  (b) The user will
     * see newly-added objects even if they don't match the search term.
     */

    NSMutableArray *matchedObjects = [NSMutableArray arrayWithCapacity:[objects count]];
  
    // split up the search string in terms, in order to do a OR-search.
    NSArray *searchTerms = [searchString componentsSeparatedByString:@" "];
    
    id currentItem;
    NSEnumerator *unfilteredObjectsEnumerator = [objects objectEnumerator];
    
    while ((currentItem = [unfilteredObjectsEnumerator nextObject])) {
        // we're allocing and autoreleasing tons of objects in this scope,
        // so having our own pool is good to avoid defragmenting memory.
        NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

        // every key usually corresponds to a table column.
        NSEnumerator *keysEnumerator = [searchKeys objectEnumerator];
        NSString *searchKey;
        
        // let's start out by cleaning up the array. we might have a bunch of attributed (styled)
        // strings, so get all raw strings before we do the search.
        NSMutableArray *cleanSearchTerms = [NSMutableArray arrayWithCapacity:[searchKeys count]];
        while ((searchKey = [keysEnumerator nextObject])) {
            NSString *cellValue = [currentItem valueForKeyPath:searchKey];
            if ([cellValue isKindOfClass:[NSAttributedString class]])
                cellValue = [(NSAttributedString *)cellValue string];
            
            [cleanSearchTerms addObject:cellValue];
        }
            
        // now do the actual AND-search on these terms, with all the given keys as search space.
        if ([FilteringArrayController searchStrings:cleanSearchTerms matchesTerms:searchTerms])
            [matchedObjects addObject:currentItem];
        
        [pool release];
    }
    
    return [super arrangeObjects:matchedObjects];
}

+ (BOOL)searchStrings:(NSArray *)searchStrings matchesTerms:(NSArray *)terms
{
    NSEnumerator *termsEnumerator = [terms objectEnumerator];
    NSString *curTerm = nil;
    while ((curTerm = [termsEnumerator nextObject])) {
        // ignore empty terms
        if ([curTerm length] == 0)
            continue;
      
        NSEnumerator *searchStringEnumerator = [searchStrings objectEnumerator];
        NSString *currentSearchString = nil;
        BOOL termFound = NO;
        while ((currentSearchString = [searchStringEnumerator nextObject])) {
            if ([currentSearchString rangeOfString:curTerm options:NSCaseInsensitiveSearch].location != NSNotFound) {
                // found the current term in this string.
                termFound = YES;
                break;
            }
        }
        
        if (termFound)
            // we had a match, so let's continue searching for the other terms.
            continue;
        
        // we couldn't find the current search term in any of the keys, so no match.
        return NO;
    }
  
    // phew, passed all tests!
    return YES;
}

- (void)dealloc
{
    [searchString release];
    searchString = nil;
    [searchKeys release];
    searchKeys = nil;
    
    [super dealloc];
}

- (NSString *)searchString
{
    return searchString;
}

- (void)setSearchString:(NSString *)newSearchString
{
    if (searchString != newSearchString) {
        [searchString autorelease];
        searchString = [newSearchString copy];
    }
}

- (void)setSearchKeys:(NSArray *)newSearchKeys
{
    if (searchKeys != newSearchKeys) {
        [searchKeys autorelease];
        searchKeys = [newSearchKeys copy];
    }
}

@end

/* Modified by Martin Hedenfalk <martin@bzero.se> for use in ShakesPeer
 * 2004-03-29: Added handling of searchKeys
 */

/*
 mmalc Crawford
 
 Copyright (c) 2004, Apple Computer, Inc., all rights reserved.
 */
/*
 IMPORTANT:  This Apple software is supplied to you by Apple Computer, Inc. ("Apple") in
 consideration of your agreement to the following terms, and your use, installation, 
 modification or redistribution of this Apple software constitutes acceptance of these 
 terms.  If you do not agree with these terms, please do not use, install, modify or 
 redistribute this Apple software.
 
 In consideration of your agreement to abide by the following terms, and subject to these 
 terms, Apple grants you a personal, non-exclusive license, under Apple’s copyrights in 
 this original Apple software (the "Apple Software"), to use, reproduce, modify and 
 redistribute the Apple Software, with or without modifications, in source and/or binary 
 forms; provided that if you redistribute the Apple Software in its entirety and without 
 modifications, you must retain this notice and the following text and disclaimers in all 
 such redistributions of the Apple Software.  Neither the name, trademarks, service marks 
 or logos of Apple Computer, Inc. may be used to endorse or promote products derived from 
 the Apple Software without specific prior written permission from Apple. Except as expressly
 stated in this notice, no other rights or licenses, express or implied, are granted by Apple
 herein, including but not limited to any patent rights that may be infringed by your 
 derivative works or by other works in which the Apple Software may be incorporated.
 
 The Apple Software is provided by Apple on an "AS IS" basis.  APPLE MAKES NO WARRANTIES, 
 EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED WARRANTIES OF NON-INFRINGEMENT, 
 MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS 
 USE AND OPERATION ALONE OR IN COMBINATION WITH YOUR PRODUCTS.
 
 IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR CONSEQUENTIAL 
 DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS 
          OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE, 
 REPRODUCTION, MODIFICATION AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED AND 
 WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING NEGLIGENCE), STRICT LIABILITY OR 
 OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
