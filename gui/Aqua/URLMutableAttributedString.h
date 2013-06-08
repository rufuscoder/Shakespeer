// vim: ft=objc
// Additions to NSMutableAttributedString to find URLs
// and mark them with the appropriate property
// Copyright (c) 2002 Aaron Sittig

#import <Cocoa/Cocoa.h>

@interface NSMutableAttributedString (URLTextViewAdditions)

- (void)detectURLs:(NSColor*)linkColor;

@end
