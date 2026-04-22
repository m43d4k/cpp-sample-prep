#include "util/native_drop_target.hpp"

#import <AppKit/AppKit.h>
#import <objc/runtime.h>

#include <utility>

std::function<void(audio_converter::util::NativeDropEvent)> g_drop_handler;

audio_converter::util::NativeDropEvent drop_event_from_pasteboard(NSPasteboard *pasteboard)
{
    audio_converter::util::NativeDropEvent event;

    NSDictionary *options = @{ NSPasteboardURLReadingFileURLsOnlyKey : @YES };
    NSArray<NSURL *> *urls = [pasteboard readObjectsForClasses:@[ [NSURL class] ] options:options];
    if (urls == nil || urls.count == 0) {
        event.error_message = "drag and drop accepts a file or folder";
        return event;
    }

    event.paths.reserve(urls.count);
    for (NSURL *url in urls) {
        if (![url isFileURL] || url.path == nil) {
            continue;
        }
        event.paths.emplace_back(url.path.UTF8String);
    }

    if (event.paths.empty()) {
        event.error_message = "drag and drop accepts a file or folder";
    }
    return event;
}

@interface AudioConverterDropDestination : NSView <NSDraggingDestination>
@end

@implementation AudioConverterDropDestination

- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)sender
{
    const auto event = drop_event_from_pasteboard(sender.draggingPasteboard);
    return event.paths.empty() ? NSDragOperationNone : NSDragOperationCopy;
}

- (NSDragOperation)draggingUpdated:(id<NSDraggingInfo>)sender
{
    return [self draggingEntered:sender];
}

- (BOOL)prepareForDragOperation:(id<NSDraggingInfo>)sender
{
    return [self draggingEntered:sender] != NSDragOperationNone;
}

- (BOOL)performDragOperation:(id<NSDraggingInfo>)sender
{
    if (!g_drop_handler) {
        return NO;
    }

    auto event = drop_event_from_pasteboard(sender.draggingPasteboard);
    g_drop_handler(std::move(event));
    return YES;
}

@end

void install_drop_methods_on_view(NSView *view)
{
    Class original_class = object_getClass(view);
    NSString *subclass_name = [NSString stringWithFormat:@"%s_AudioConverterDropDestination", class_getName(original_class)];
    Class drop_class = NSClassFromString(subclass_name);
    if (drop_class == Nil) {
        drop_class = objc_allocateClassPair(original_class, subclass_name.UTF8String, 0);
        if (drop_class == Nil) {
            return;
        }

        const SEL selectors[] = {
            @selector(draggingEntered:),
            @selector(draggingUpdated:),
            @selector(prepareForDragOperation:),
            @selector(performDragOperation:),
        };

        for (SEL selector : selectors) {
            Method method = class_getInstanceMethod([AudioConverterDropDestination class], selector);
            class_addMethod(drop_class, selector, method_getImplementation(method), method_getTypeEncoding(method));
        }

        objc_registerClassPair(drop_class);
    }

    if (object_getClass(view) != drop_class) {
        object_setClass(view, drop_class);
    }

    [view registerForDraggedTypes:@[ NSPasteboardTypeFileURL ]];
}

@interface AudioConverterDropMonitor : NSObject
- (void)startMonitoring;
- (void)attachToOpenWindows;
- (void)attachToWindow:(NSWindow *)window;
- (void)windowDidBecomeKey:(NSNotification *)notification;
@end

@implementation AudioConverterDropMonitor

- (void)startMonitoring
{
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(windowDidBecomeKey:)
                                                 name:NSWindowDidBecomeKeyNotification
                                               object:nil];
    [self performSelector:@selector(attachToOpenWindows) withObject:nil afterDelay:0];
}

- (void)windowDidBecomeKey:(NSNotification *)notification
{
    NSWindow *window = notification.object;
    if ([window isKindOfClass:[NSWindow class]]) {
        [self attachToWindow:window];
    }
}

- (void)attachToOpenWindows
{
    for (NSWindow *window in NSApp.windows) {
        [self attachToWindow:window];
    }
}

- (void)attachToWindow:(NSWindow *)window
{
    if (window.contentView != nil) {
        install_drop_methods_on_view(window.contentView);
    }
}

@end

AudioConverterDropMonitor *g_drop_monitor = nil;

namespace audio_converter::util {

bool install_native_file_drop_handler(std::function<void(NativeDropEvent)> handler, std::string &error_message)
{
    error_message.clear();
    g_drop_handler = std::move(handler);

    if (NSApp == nil) {
        error_message = "failed to access the macOS application object";
        return false;
    }

    if (g_drop_monitor == nil) {
        g_drop_monitor = [AudioConverterDropMonitor new];
        [g_drop_monitor startMonitoring];
    }

    return true;
}

} // namespace audio_converter::util
