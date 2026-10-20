/* Copied from the mh1-xbox repo (src/pc/xbox/testlaunch, same owner) for tools/xemu_loader_test.sh. */
/* xemu test aid (tools/xemu_loader_test.sh): boots from the test DVD and starts the XBE named in
 * E:\launch.txt (e.g. E:\Games\MHG\default.xbe), so the network loader runs from its folder on the
 * hard disk as on the real Xbox (Evolution X starts it from there). It deletes launch.txt first, so on
 * a later boot in the same session (the program it started went "back to the dashboard": xemu then
 * boots this DVD again) it only says so and waits: a test run ends there instead of looping. (The
 * launch data type can't tell the two apart: xemu's cold boot already reports 2.) Never installed on
 * hardware. */
#include <hal/debug.h>
#include <hal/video.h>
#include <hal/xbox.h>
#include <nxdk/mount.h>
#include <windows.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    char path[200] = "";
    FILE *f;

    XVideoSetMode(640, 480, 32, REFRESH_DEFAULT);
    nxMountDrive('E', "\\Device\\Harddisk0\\Partition1\\");
    if ((f = fopen("E:\\launch.txt", "r")) != NULL) {
        if (fgets(path, sizeof path, f))
            path[strcspn(path, "\r\n")] = 0;
        fclose(f);
        remove("E:\\launch.txt");
    }
    if (!path[0]) {
        debugPrint("testlaunch: no E:\\launch.txt (any more): the test run is over.\n");
        for (;;)
            Sleep(1000);
    }
    debugPrint("testlaunch: starting %s\n", path);
    Sleep(500);
    XLaunchXBE(path);
    debugPrint("testlaunch: could not start it.\n");
    for (;;)
        Sleep(1000);
}
