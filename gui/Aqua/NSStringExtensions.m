/* vim: ft=objc
 *
 * Copyright 2005 Martin Hedenfalk <martin@bzero.se>
 *
 * This file is part of ShakesPeer.
 *
 * ShakesPeer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * ShakesPeer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ShakesPeer; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#import "NSStringExtensions.h"

@implementation NSString (ShakesPeerExtensions)

- (NSComparisonResult)numericalCompare:(NSString *)aString
{
    return [self compare:aString options:NSNumericSearch];
}

- (NSString *)stringByDeletingLastWindowsPathComponent
{
    NSCharacterSet *backslashSet = [NSCharacterSet characterSetWithCharactersInString:@"\\"];
    NSRange r = [self rangeOfCharacterFromSet:backslashSet options:NSBackwardsSearch];
    
    if (r.location == NSNotFound)
        return [self stringByDeletingLastPathComponent];
    
    return [self substringToIndex:r.location];
}

- (NSString *)lastWindowsPathComponent
{
    NSCharacterSet *backslashSet = [NSCharacterSet characterSetWithCharactersInString:@"\\"];
    NSRange r = [self rangeOfCharacterFromSet:backslashSet options:NSBackwardsSearch];
    
    if (r.location == NSNotFound)
        return [self lastPathComponent];
    
    return [self substringFromIndex:r.location+1];
}

- (int)indexOfString:(NSString *)substring
{
    return [self rangeOfString:substring options:0].location;
}

- (int)indexOfCharacter:(unichar)character fromIndex:(int)startIndex
{
    NSRange chars;
    NSCharacterSet *charSet;

    chars.location = character;
    chars.length = 1;
    charSet = [NSCharacterSet characterSetWithRange:chars];

    return [self rangeOfCharacterFromSet:charSet options:0].location;
}

- (int)occurencesOfString:(NSString *)aString
{
    NSRange range = NSMakeRange(0, [self length]);
    int found = 0;

    while (1) {
        NSRange foundRange = [self rangeOfString:aString options:NSLiteralSearch range:range];
        if (foundRange.location == NSNotFound)
            break;
        found++;
        range.location = foundRange.location + foundRange.length;
        range.length = [self length] - range.location;
        if (range.length <= 0)
            break;
    }

    return found;
}

- (uint64_t)unsignedLongLongValue
{
    NSScanner *scanner = [NSScanner scannerWithString:self];
    int64_t ullValue;
    [scanner scanLongLong:&ullValue];
    
    return (uint64_t)ullValue;
}

- (NSAttributedString *)truncatedString:(NSLineBreakMode)mode
{
    NSMutableParagraphStyle *style = [[[NSMutableParagraphStyle alloc] init] autorelease];
    [style setLineBreakMode:mode];
    NSMutableAttributedString *truncatedString = [[NSMutableAttributedString alloc] initWithString:self];
    [truncatedString addAttribute:NSParagraphStyleAttributeName
                            value:style
                            range:NSMakeRange(0, [self length])];
    
    return truncatedString; /* not autoreleased */
}

- (NSString *)stringWithoutWhitespace
{
    return [self stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
}

+ (NSString *)stringWithNowPlayingMessage {
    // Display the current playing track in iTunes
    
    // Define a base message, which will be returned if there's an error, or if 
    // nothing is playing.
    NSString *theMessage = @"/me isn't listening to anything";
    NSString *path = [[NSBundle mainBundle] pathForResource:@"np" ofType:@"scpt"];
    if (path != nil) {
        // Create the URL for the script
        NSURL* url = [NSURL fileURLWithPath:path];
        if (url != nil) {
            // Set up an error dict and the script
            NSDictionary *errors;
            NSAppleScript* appleScript = [[NSAppleScript alloc] initWithContentsOfURL:url error:&errors];
            if (appleScript != nil) {
                // Run the script
                NSAppleEventDescriptor *returnDescriptor = [appleScript executeAndReturnError:&errors];
                [appleScript release];
                if (returnDescriptor != nil) {
                    // We got some results
                    NSString *theTitle = [[returnDescriptor descriptorAtIndex:1] stringValue];
                    NSString *theArtist = [[returnDescriptor descriptorAtIndex:2] stringValue];
                    if (theTitle && theArtist)
                        theMessage = [NSString stringWithFormat:@"/me is listening to %@ by %@", theTitle, theArtist];
                } else {
                    // Something went wrong
                    NSLog(@"Script error: %@", [errors objectForKey: @"NSAppleScriptErrorMessage"]);
                } // returndescriptor
            } // applescript
        } // url
    } // path
    return theMessage;
}

@end

@implementation NSMutableAttributedString (ShakesPeerExtensions)

- (NSComparisonResult)compare:(NSMutableAttributedString *)aString
{
    return [[self string] compare:[aString string]];
}

@end

