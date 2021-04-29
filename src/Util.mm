#include "Util.h"
#include "Window.h"

#include <fstream>

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>

#ifdef DSIP_GUI

static Window* s_MainWindow = nullptr;

@interface AppPreferences : NSObject
+ (void)setup;
+ (void)openImageFile;
+ (void)saveImageFile;
+ (void)openProfile;
+ (void)saveProfile;
+ (NSURL*)openFilePanel;
@end

@implementation AppPreferences

+ (void)setup
{
    NSMenu* main_menu = [[NSApplication sharedApplication] mainMenu];

    NSMenuItem* file_menu_item = [main_menu insertItemWithTitle:@"" action:nil keyEquivalent:@"" atIndex:1];
    NSMenu* file_menu = [[NSMenu alloc] init];
    
    [file_menu_item setSubmenu:file_menu];

    [file_menu setTitle:@"File"];
    [file_menu addItemWithTitle:@"Open Image File" action:@selector(openImageFile) keyEquivalent:@"o"].target = self;
    [file_menu addItemWithTitle:@"Save Image File" action:@selector(saveImageFile) keyEquivalent:@"s"].target = self;
    [file_menu addItemWithTitle:@"Open Profile" action:@selector(openProfile) keyEquivalent:@""].target = self;
    [file_menu addItemWithTitle:@"Save Profile" action:@selector(saveProfile) keyEquivalent:@""].target = self;
}

+ (NSURL*)openFilePanel
{
    NSOpenPanel* open_panel = [NSOpenPanel openPanel];
    open_panel.canChooseFiles = YES;
    open_panel.canChooseDirectories = NO;
    open_panel.allowsMultipleSelection = NO;
    NSInteger button = [open_panel runModal];
    if (button == NSModalResponseOK) {
        NSURL* url = [open_panel URLs].firstObject;
        return url;
    }
    return nil;
}

+ (void)openImageFile
{
    NSURL* url = [AppPreferences openFilePanel];
    if (url != nil)
    {
        std::string path([[url path] UTF8String]);
        s_MainWindow->LoadImage(path);
    }
}

+ (void)saveImageFile
{
    NSURL* url = [AppPreferences openFilePanel];
    if (url != nil)
    {
        std::string path([[url path] UTF8String]);
        s_MainWindow->SaveImage(path);
    }
}

+ (void)openProfile
{
    NSURL* url = [AppPreferences openFilePanel];
    if (url != nil)
    {
        std::string path([[url path] UTF8String]);
        s_MainWindow->LoadProfile(path);
    }
}

+ (void)saveProfile
{
    NSURL* url = [AppPreferences openFilePanel];
    if (url != nil)
    {
        std::string path([[url path] UTF8String]);
        s_MainWindow->SaveProfile(path);
    }
}

@end

#endif

namespace Util
{

#ifdef DSIP_GUI

void SetupPlatformMenu(Window* window)
{
    s_MainWindow = window;
    [AppPreferences setup];
}

NSString* ResolvePath(const char* path)
{
    NSString* ns_path = [NSString stringWithUTF8String:path];
    NSString* resources = [[NSBundle mainBundle] resourcePath];
    return [resources stringByAppendingPathComponent:ns_path];
}

std::vector<char> BytesFromBundle(const char* file_path)
{
    NSString* path = ResolvePath(file_path);
    return BytesFromFile([path UTF8String]);
}

#endif

std::vector<char> BytesFromFile(const char* file_path)
{
    std::ifstream file(file_path, std::ifstream::ate | std::ifstream::binary);
    std::vector<char> data;
    if (file.is_open())
    {
        uint32_t size = file.tellg();
        data.resize(size);
        file.seekg(std::ifstream::beg);
        file.read(&data[0], size);
    }
    return data;
}

}
