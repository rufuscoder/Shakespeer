/*
 * Copyright 2006 Markus Magnuson <markus@konstochvanligasaker.se>
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
 *
 * Part of this code is from Adium (AIKeychain.m)
 * Copyright (C) 2001-2005, Adam Iser (adamiser@mac.com | http://www.adiumx.com)
 */

#import "SPKeychain.h"
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#include <CoreServices/CoreServices.h>

@implementation SPKeychain

// full version of the addPassword function
+ (void)addPassword:(NSString *)password
          forServer:(NSString *)server
            account:(NSString *)account
       keychainItem:(out SecKeychainItemRef *)outKeychainItem
              error:(out NSError **)outError
{
    NSParameterAssert(password != nil);
    NSParameterAssert(server != nil);
    NSParameterAssert(account != nil);
    
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    
    NSString *passwordStr = [NSString stringWithString:password];
    NSData *passwordData = [passwordStr dataUsingEncoding:NSUTF8StringEncoding];
    
    NSData *serverData = [server dataUsingEncoding:NSUTF8StringEncoding];
    NSData *accountData = [account dataUsingEncoding:NSUTF8StringEncoding];
    
    OSStatus err = SecKeychainAddInternetPassword(NULL,                     // use default keychain
                                                  [serverData length],      // length of server name
                                                  [serverData bytes],       // server name
                                                  0,                        // length of domain
                                                  NULL,                     // domain
                                                  [accountData length],     // length of nickname
                                                  [accountData bytes],      // nickname
                                                  0,                        // length of path
                                                  NULL,                     // path
                                                  0,                        // port
                                                  0,                        // protocol
                                                  kSecAuthenticationTypeDefault, // authentication type
                                                  [passwordData length],    // length of password
                                                  [passwordData bytes],     // password
                                                  outKeychainItem);         // item reference
    
    [pool release];
    
    if (outError) {
        NSError *error = nil;
        if (err != noErr)
            error = [NSError errorWithDomain:NSOSStatusErrorDomain code:err userInfo:nil];
        *outError = error;
    }
}

// convenient version of the addPassword function
+ (void)addPassword:(NSString *)password
          forServer:(NSString *)server
            account:(NSString *)account
              error:(out NSError **)outError
{
    [SPKeychain addPassword:password
                  forServer:server
                    account:account
               keychainItem:NULL
                      error:outError];
}

// full version of the findPassword function
+ (NSString *)findPasswordForServer:(NSString *)server
                            account:(NSString *)account
                       keychainItem:(out SecKeychainItemRef *)outKeychainItem
                              error:(out NSError **)outError
{
    void  *passwordData = NULL;
    UInt32 passwordLength = 0;
    
    NSData *serverData = [server dataUsingEncoding:NSUTF8StringEncoding];
    NSData *accountData = [account dataUsingEncoding:NSUTF8StringEncoding];
    
    OSStatus err = SecKeychainFindInternetPassword(NULL,                    // use default keychain
                                                   [serverData length],     // length of server name
                                                   [serverData bytes],      // server name
                                                   0,                       // length of domain
                                                   NULL,                    // domain
                                                   [accountData length],    // length of nickname
                                                   [accountData bytes],     // nickname
                                                   0,                       // length of path
                                                   NULL,                    // path
                                                   0,                       // port
                                                   0,                       // protocol
                                                   kSecAuthenticationTypeDefault, // authentication type
                                                   &passwordLength,         // length of password
                                                   &passwordData,           // password
                                                   outKeychainItem);        // item reference
    
    NSString *passwordString = [[[NSString alloc] initWithBytes:passwordData
                                                         length:passwordLength
                                                       encoding:NSUTF8StringEncoding] autorelease];
    
    SecKeychainItemFreeContent(NULL, passwordData);
    
    if (outError) {
        NSError *error = nil;
        if (err != noErr)
            error = [NSError errorWithDomain:NSOSStatusErrorDomain code:err userInfo:nil];
        *outError = error;
    }
    
    return passwordString;
}

// convenient version of the findPassword function
+ (NSString *)passwordForServer:(NSString *)server
                        account:(NSString *)account
                          error:(out NSError **)outError
{
    NSString *password = [SPKeychain findPasswordForServer:server
                                                   account:account
                                              keychainItem:NULL
                                                     error:outError];
    
    return password;
}

+ (void)setPassword:(NSString *)password
          forServer:(NSString *)server
            account:(NSString *)account
       keychainItem:(out SecKeychainItemRef *)outKeychainItem
              error:(out NSError **)outError
{
    if (!password) {
        // password is empty, so remove it
        [SPKeychain deletePasswordForServer:server
                                    account:account
                               keychainItem:outKeychainItem
                                      error:outError];
    }
    else {
        // add the password if it doesn't exist
        NSError *error = nil;
        
        [SPKeychain addPassword:password
                      forServer:server
                        account:account
                   keychainItem:outKeychainItem
                          error:&error];
        
        if (error) {
            OSStatus err = [error code];
            
            if (err == errSecDuplicateItem) {
                // a password already exists, so change that one instead
                SecKeychainItemRef item = NULL;
                NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
                
                [SPKeychain findPasswordForServer:server
                                          account:account
                                     keychainItem:&item
                                            error:&error];
                
                [(NSObject *)item autorelease];
                
                if (error) {
                    if (outError)
                        *outError = error;
                }
                else {
                    NSString *passwordStr = [NSString stringWithString:password];
                    NSData *passwordData = [passwordStr dataUsingEncoding:NSUTF8StringEncoding];
                    
                    // change the password
                    SecKeychainItemModifyAttributesAndData(item,
                                                           NULL, // attributes
                                                           [passwordData length],
                                                           [passwordData bytes]);
                    
                }
                
                [pool release];
            }
        }
    }
}

// convenient version of the setPassword function
+ (void)setPassword:(NSString *)password
          forServer:(NSString *)server
            account:(NSString *)account
              error:(out NSError **)outError
{
    [SPKeychain setPassword:password
                  forServer:server
                    account:account
               keychainItem:NULL
                      error:outError];
}

// full version of the deletePassword function
+ (void)deletePasswordForServer:(NSString *)server
                        account:(NSString *)account
                   keychainItem:(out SecKeychainItemRef *)outKeychainItem
                          error:(out NSError **)outError
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    
    SecKeychainItemRef keychainItem = NULL;
    NSError *error = nil;
    
    [SPKeychain findPasswordForServer:server
                              account:account
                         keychainItem:&keychainItem
                                error:&error];
    
    if (keychainItem)
        SecKeychainItemDelete(keychainItem);
    
    if (outKeychainItem)
        *outKeychainItem = keychainItem;
    else if (keychainItem)
        CFRelease(keychainItem);
    
    [pool release];
}

// convenient version of the deletePassword function
+ (void)deletePasswordForServer:(NSString *)server
                        account:(NSString *)account
                          error:(out NSError **)outError
{
    [SPKeychain deletePasswordForServer:server
                                account:account
                           keychainItem:NULL
                                  error:outError];
}

@end