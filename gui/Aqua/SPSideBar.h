//  vim: ft=objc
//  SPSideBar.h
//  MacDCPlusPlus
//
//  Created by Jonathan Jansson on Sun Apr 11 2004.
//  Copyright (c) 2004 __MyCompanyName__. All rights reserved.
//

#import <Cocoa/Cocoa.h>

/*!
    @class SPSideBar
    @discussion SPSideBar is used for controlling the connection between
    the SPSideBar and its NSTabView. 
 
    Don't use the NSTabView directly. If you want to show an item in
    the tab view without adding it to the sidebar just use showItem:
    without adding it first.
 
    All items must respond to -(NSView *)view or it will probably crash.
    Items that will appear in the sidebar should have the key
    or methods -(NSImage *)image and -(NSString *)title. */

@protocol SPSideBarItem

- (NSView *)view;
- (NSString *)title;
- (NSImage *)image;
- (NSMenu *)menu;

@end

@interface NSObject (SPSideBarItemInformalProtocol)

// These methods can be implemented if a sidebar item wants to be
// notified when it is selected and deselected. For example, this can
// be useful if you want to invalidate a timer when the item is
// deselected, and restart the timer when it is selected again; this
// was first implemented in SPFriendsController, which you can check
// out for further enlightenment.
- (void)viewBecameSelected;
- (void)viewBecameDeselected;

- (void)unbindControllers;
- (NSString *)sectionTitle;
- (BOOL)canClose;
- (BOOL)isHighlighted;
- (void)setHighlighted:(BOOL)aFlag;

// return data types for drag & drop for which the sidebar item
// should be highlighted 
- (NSArray *)interestingDropTypes;

@end

@interface SPSideBar : NSTableView
{
    IBOutlet NSTabView *tabView;
    id delegate;
    NSMutableArray *items;
    
    // only set when there's a target being dragged over right now.
    NSTimer *dropTimer;
}

/*!
    @method addObject:
    @discussion Adds an object to the array controller and shows the object in
    a tab view. It will not select it in the list or bring it to front.
 */
- (void)addItem:(id <SPSideBarItem>)anItem;
- (void)addItem:(id <SPSideBarItem>)anItem toSection:(NSString *)aSection;
- (void)addSection:(NSString *)aSection;
- (BOOL)hasSection:(NSString *)aSection;
- (unsigned int)numberOfItemsInSection:(NSString *)aSection;
- (NSArray *)itemsInSection:(NSString *)aSection;
- (void)setDelegate:(id)aDelegate;
- (id)itemWithTitle:(NSString *)aTitle;

/*!
    @method displayItem:
    @discussion Used to bring an item in the SPSideBar to front or temporarily
    show some other item in the NSTabView.
 */
- (void)displayItem:(id)anItem;

/*!
    @method removeObject:
    @discussion Removes an object from the array controller and the tab view.
 */
- (void)removeItem:(id)anItem;

/*!
    @method closeSelectedItem:
    @discussion Remove the currently selected item. This is called from the
    close button in the table. Could also be made to an IBAction if necessary.
 */
- (void)closeSelectedItem:(id)sender;

- (void)selectPreviousItem;
- (void)selectNextItem;

@end

@protocol SPSideBarDelegate
- (void)sideBar:(SPSideBar *)sideBar didDeselectItem:(id)anItem;
- (void)sideBar:(SPSideBar *)sideBar didSelectItem:(id)anItem;
- (void)sideBar:(SPSideBar *)sideBar willCloseItem:(id)sideBarItem;
- (void)sideBar:(SPSideBar *)sideBar didCloseItem:(id)sideBarItem;
@end

