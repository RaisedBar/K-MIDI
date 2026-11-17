// macOS implementation of the screen reader helpers, using AppKit's NSAccessibility (VoiceOver).
#include "Announce.h"

#import <AppKit/AppKit.h>

namespace a11y {

namespace {

NSString* toNS(const wxString& s) {
    NSString* result = [NSString stringWithUTF8String:s.utf8_str().data()];
    return result ? result : @"";
}

// Labels a view and, for a scroll view (as a table is wrapped in), the document view inside it, which is what
// VoiceOver actually focuses.
void labelView(NSView* view, NSString* label) {
    if (!view) return;
    [view setAccessibilityLabel:label];
    if ([view isKindOfClass:[NSScrollView class]]) {
        NSView* document = [(NSScrollView*)view documentView];
        if (document) [document setAccessibilityLabel:label];
    }
}

}  // namespace

void announce(wxWindow*, const wxString& text) {
    if (text.empty()) return;
    NSDictionary* info = @{
        NSAccessibilityAnnouncementKey : toNS(text),
        NSAccessibilityPriorityKey : @(NSAccessibilityPriorityHigh),
    };
    NSAccessibilityPostNotificationWithUserInfo(NSApp, NSAccessibilityAnnouncementRequestedNotification, info);
}

void setName(wxWindow* window, const wxString& name) {
    if (!window) return;
    NSString* label = toNS(name);
    labelView((NSView*)window->GetHandle(), label);
    // Some wx controls are made of several native views (wxSpinCtrl is a text field and a stepper), and VoiceOver
    // focuses the inner ones, so they are labelled too.
    for (wxWindow* child : window->GetChildren()) setName(child, name);
}

}  // namespace a11y
