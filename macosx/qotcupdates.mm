/*
 *  Quest of the Companions updates on macOS, through Sparkle.
 *  See qotcupdates.h and docs/friend-mode/updates.md.
 */

#include "qotcupdates.h"

#import <AppKit/AppKit.h>
#import <Sparkle/Sparkle.h>

#include <cstdlib>

// QOTC_UPDATE_FEED, when set, replaces the feed in Info.plist: for trying
// an update against a local test feed (docs/friend-mode/updates.md).
@interface QOTCUpdaterDelegate : NSObject <SPUUpdaterDelegate>
@end

@implementation QOTCUpdaterDelegate
- (nullable NSString*)feedURLStringForUpdater:(SPUUpdater*)updater {
	(void)updater;
	const char* feed = std::getenv("QOTC_UPDATE_FEED");
	return feed != nullptr && *feed != '\0' ? @(feed) : nil;
}
@end

static QOTCUpdaterDelegate*          updater_delegate;
static SPUStandardUpdaterController* updater_controller;

void QOTC_start_updates() {
	if (updater_controller != nil
		|| [[NSBundle mainBundle] objectForInfoDictionaryKey:@"SUPublicEDKey"] == nil) {
		return;
	}
	updater_delegate   = [[QOTCUpdaterDelegate alloc] init];
	updater_controller = [[SPUStandardUpdaterController alloc]
			initWithStartingUpdater:YES
					updaterDelegate:updater_delegate
				 userDriverDelegate:nil];

	// "Check for Updates..." under "About" in the app menu SDL made.
	NSMenu* app_menu = [[[NSApp mainMenu] itemAtIndex:0] submenu];
	if (app_menu != nil) {
		NSMenuItem* item = [[NSMenuItem alloc]
				initWithTitle:@"Check for Updates…"
					   action:@selector(checkForUpdates:)
				keyEquivalent:@""];
		item.target         = updater_controller;
		const NSInteger at = app_menu.numberOfItems > 0 ? 1 : 0;
		[app_menu insertItem:item atIndex:at];
	}
}

bool QOTC_updates_on() {
	return updater_controller != nil;
}

void QOTC_check_for_updates() {
	if (updater_controller != nil) {
		[updater_controller checkForUpdates:nil];
	}
}
