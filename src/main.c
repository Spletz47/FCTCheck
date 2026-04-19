#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <coreinit/thread.h>
#include <coreinit/mcp.h>
#include <coreinit/filesystem.h>
#include <coreinit/filesystem_fsa.h>

#include <sndcore2/core.h>

#include <whb/log.h>
#include <whb/log_console.h>
#include <whb/proc.h>

#include <mocha/mocha.h>

/// \brief Macro to generate an MCP app type value.
/// \param  flags  s64
/// \param  id     s64
//
#define MCP_MAKE_APP_TYPE(flags, id) ((flags) | ((id) & ((1<<24) - 1)))

#define MCP_APP_TYPE_FCT (MCP_MAKE_APP_TYPE((1 << 27), 42))

/// \brief Everything except FS_ERROR_FLAG_EXISTS
#define FS_ERROR_FLAG_MY (FS_ERROR_FLAG_MAX | FS_ERROR_FLAG_ALREADY_OPEN | FS_ERROR_FLAG_NOT_FOUND | FS_ERROR_FLAG_NOT_FILE | FS_ERROR_FLAG_NOT_DIR | FS_ERROR_FLAG_PERMISSION_ERROR | FS_ERROR_FLAG_FILE_TOO_BIG | FS_ERROR_FLAG_STORAGE_FULL | FS_ERROR_FLAG_UNSUPPORTED_CMD | FS_ERROR_FLAG_JOURNAL_FULL)

void copyFile(FSClient *client, FSCmdBlock *block, const char *basePath, const char *relPath, const char *dstDir) {
    char srcPath[256];
    char dstPath[256];
    
    const char *filename = strrchr(relPath, '/');
    filename = filename ? filename + 1 : relPath;
    
    snprintf(srcPath, sizeof(srcPath), "%s%s", basePath, relPath);
    snprintf(dstPath, sizeof(dstPath), "%s/%s", dstDir, filename);

    FSStat stat;
    int res = FSGetStat(client, block, srcPath, &stat, FS_ERROR_FLAG_ALL);
    if (res < 0) { WHBLogPrintf("FSGetStat() failed for %s: %d", srcPath, res); return; }

    void *buffer = malloc(stat.size);

    FSFileHandle srcHandle;
    res = FSOpenFile(client, block, srcPath, "r", &srcHandle, FS_ERROR_FLAG_ALL);
    if (res < 0) { WHBLogPrintf("FSOpenFile() failed for %s: %d", srcPath, res); return; }
    res = FSReadFile(client, block, buffer, 1, stat.size, srcHandle, 0, FS_ERROR_FLAG_ALL);
    FSCloseFile(client, block, srcHandle, FS_ERROR_FLAG_NONE);
    if (res < 0) { WHBLogPrintf("FSReadFile() failed for %s: %d", srcPath, res); return; }

    FSFileHandle dstHandle;
    res = FSOpenFile(client, block, dstPath, "w", &dstHandle, FS_ERROR_FLAG_MY);
    if (res < 0) { WHBLogPrintf("FSOpenFile() failed for %s: %d", dstPath, res); return; }
    res = FSWriteFile(client, block, buffer, 1, stat.size, dstHandle, 0, FS_ERROR_FLAG_MY);
    FSCloseFile(client, block, dstHandle, FS_ERROR_FLAG_NONE);
    if (res < 0) { WHBLogPrintf("FSWriteFile() failed for %s: %d", dstPath, res); return; }

    WHBLogPrintf("Successfully copied %s to SD card.", filename);
    WHBLogConsoleDraw();
}

void fctcheck_impl() {
// --- MCP title lookup ---
            int handle = MCP_Open();
 
            uint32_t count = 0;
            MCPTitleListType *titles = malloc(sizeof(MCPTitleListType));

            if (handle < 0) { WHBLogPrintf("Failed to open MCP: %d", handle); return; }
 
            int res = MCP_TitleListByAppType(handle, MCP_APP_TYPE_FCT, &count, titles, sizeof(MCPTitleListType));
            MCP_Close(handle);

            if (res < 0) { WHBLogPrintf("MCP_TitleListByAppType() failed: %d", res); return; }

            if (count == 0) { WHBLogPrintf("No titles found. You may now exit the application."); return; }
 
            WHBLogPrintf("Found FCT title");
            WHBLogPrintf("Title ID:    0x%016llX", titles[0].titleId);
            WHBLogPrintf("SDK Version: %u.%u.%02u", titles[0].sdkVersion / 10000, (titles[0].sdkVersion / 100) % 100, titles[0].sdkVersion % 100);
            WHBLogPrintf("Path:        %s", titles[0].path);
            WHBLogPrintf("\n");
            WHBLogConsoleDraw();

            // --- File copy ---
            FSClient *client = malloc(sizeof(FSClient));
            FSCmdBlock *block = malloc(sizeof(FSCmdBlock));
 
            FSInit();
            Mocha_InitLibrary();
            res = FSAddClient(client, FS_ERROR_FLAG_ALL); if (res < 0) { WHBLogPrintf("FSAddClient() failed: %d", res); return; }
            Mocha_UnlockFSClient(client);
            FSInitCmdBlock(block);

            res = FSMakeDir(client, block, "/vol/external01/FCTCheck-Dump", FS_ERROR_FLAG_ALL);
            if (res < 0 && res != FS_STATUS_EXISTS) { WHBLogPrintf("FSMakeDir() failed: %d", res); return; }

            // Create dump directory for title on SD card.
            // This allows for the dumping of multiple titles if the user is using the same SD card to check multiple consoles.
            char dstDir[64];
            snprintf(dstDir, sizeof(dstDir), "/vol/external01/FCTCheck-Dump/%016llX", titles[0].titleId);

            res = FSMakeDir(client, block, dstDir, FS_ERROR_FLAG_ALL);
            if (res == FS_STATUS_EXISTS) {
               WHBLogPrintf("Dump directory for title already exists. Proceeding...");
               WHBLogConsoleDraw();
            }
            else if (res < 0) { WHBLogPrintf("FSMakeDir() failed: %d", res); return; }

           const char *basePath = titles[0].path;

            copyFile(client, block, basePath, "/content/layout.txt", dstDir);
            copyFile(client, block, basePath, "/content/layout_caffeine.txt", dstDir);
            copyFile(client, block, basePath, "/code/app.xml", dstDir);

            FSDelClient(client, FS_ERROR_FLAG_NONE);
            FSShutdown();
            return;
}



int main(int argc, char **argv)
{
   bool checked = false;

   WHBProcInit();
   AXInit();

   WHBLogConsoleInit();
   WHBLogConsoleSetColor(0x3A5A40FF);
   WHBLogPrintf("Welcome to FCT checker by Spletz");
   WHBLogPrintf("This program will check for any FCT titles");
   WHBLogPrintf("\n");
   WHBLogConsoleDraw();

   while (WHBProcIsRunning()) {
      if (!checked) {
         
         fctcheck_impl();
         WHBLogPrintf("Finished.");
         WHBLogPrintf("\n");
         WHBLogPrintf("Press the HOME button to exit.");
         checked = true;
      }
      WHBLogConsoleDraw();
    }

   WHBLogPrintf("Exiting...");
   WHBLogConsoleDraw();
   OSSleepTicks(OSMillisecondsToTicks(1000));

   WHBLogConsoleFree();
   WHBProcShutdown();
   return 0;
}
