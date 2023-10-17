/* Copyright (C)
* 2021 - Christoph van Wüllen, DL1YCF
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <https://www.gnu.org/licenses/>.
*
*/

/*
 * File MacOS.c
 *
 * This file need only be compiled on MacOS.
 * It contains some functions only needed there:
 *
 * MaOSstartup(char *path)      : create working dir in "$HOME/Library/Application Support" etc.
 * apple_sem(int init)          : return a pointer to a semaphore
 *
 * where *path is argv[0], the file name of the running executable
 *
 */

#ifdef __APPLE__

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <pwd.h>
#include <uuid/uuid.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/pwr_mgt/IOPMLib.h>

void MacOSstartup(const char *path) {
  //
  // We used to do this from a wrapper shell script.
  // However, this confuses MacOS when it comes to
  // granting access to the local microphone therefore
  // it is now built into the binary
  //
  // We have to distinguish two basic situations:
  //
  // a) piHPSDR is called from a command line
  // b) piHPSDR is within an app bundle
  //
  // In case a) nothing has to be done here (standard UNIX),
  // but in case b) we do the following:
  //
  // - if not yet existing, create the directory "$HOME/Library/Application Support/piHPSDR"
  // - copy the icon to that directory
  // - chdir to that directory
  //
  // Case b) is present if the current working directory is "/".
  //
  // If "$HOME/Library/Application Support" does not exist, fall back to case a)
  //
  char workdir[1024];
  char *homedir;
  const char *AppSupport = "/Library/Application Support/piHPSDR";
  struct stat st;
  static IOPMAssertionID keep_awake = 0;
  //
  //  This is to prevent "going to sleep" or activating the screen saver
  //  while piHPSDR is running
  //
  //  works from macOS 10.6 so should be enough
  //  no return check is needed
  IOPMAssertionCreateWithName (kIOPMAssertionTypeNoDisplaySleep, kIOPMAssertionLevelOn,
                               CFSTR ("piHPSDR"), &keep_awake);
  //
  //  If the current work dir is NOT "/", just do nothing
  //
  *workdir = 0;

  if (getcwd(workdir, sizeof(workdir)) == NULL) { return; }

  if (strlen(workdir) != 1 || *workdir != '/') { return; }

  //
  //  Determine working directory,
  //  "$HOME/Library/Application Support/piHPSDR"
  //  and create it if it does not exist
  //  take care to enclose name with quotation marks
  //
  if ((homedir = getenv("HOME")) == NULL) {
    homedir = getpwuid(getuid())->pw_dir;
  }

  if (strlen(homedir) + strlen(AppSupport) > 1020) { return; }

  strlcpy(workdir, homedir, 1024);
  strlcat(workdir, AppSupport, 1024);

  //
  //  Check if working dir exists, otherwise try to create it
  //
  if (stat(workdir, &st) < 0) {
    mkdir(workdir, 0755);
  }

  //
  //  Is it there? If not, give up
  //
  if (stat(workdir, &st) < 0) {
    return;
  }

  //
  //  Is it a directory? If not, give up
  //
  if ((st.st_mode & S_IFDIR) == 0) {
    return;
  }

  //
  //  chdir to the work dir
  //
  if (chdir(workdir) != 0) {
    return;
  }

  //
  //  Make two local files for stdout and stderr, to allow
  //  post-mortem debugging
  //
  (void) freopen("pihpsdr.stdout", "w", stdout);
  (void) freopen("pihpsdr.stderr", "w", stderr);

  //
  //  Copy icon from app bundle to the work dir
  //
  if (getcwd(workdir, sizeof(workdir)) != NULL && strlen(path) < 1024) {
    //
    // source = basename of the executable
    //
    char *c;
    char source[1024];
    strlcpy(source, path, 1024);
    c = rindex(source, '/');

    if (c) {
      *c = 0;
      const char *IconInApp  = "/../Resources/hpsdr.png";

      if ((strlen(source) + strlen(IconInApp) < 1024)) {
        int fdin, fdout;
        strlcat(source,  IconInApp, 1024);
        //
        // Now copy the file from "source" to "workdir"
        //
        fdin = open(source, O_RDONLY);
        fdout = open("hpsdr.png", O_WRONLY | O_CREAT | O_TRUNC, (mode_t) 0400);

        if (fdin >= 0 && fdout >= 0) {
          //
          // Now do the copy, use "source" as I/O buffer
          //
          int ch;

          while ((ch = read(fdin, source, 1024)) > 0) {
            write (fdout, source, ch);
          }

          close(fdin);
          close(fdout);
        }
      }
    }
  }
}

/////////////////////////////////////////////////////////////////////////////
//  
// MacOS semaphores
//  
// Since MacOS only supports named semaphores, we have to be careful to
// allow serveral instances of this program to run at the same time on the
// same machine
//
/////////////////////////////////////////////////////////////////////////////
#include <semaphore.h>
#include <errno.h>
#include <message.h>

sem_t *apple_sem(int initial_value) {
  sem_t *sem;
  static long semcount = 0;
  char sname[20];

  for (;;) {
    snprintf(sname, 20, "PI_%08ld", semcount++);
    sem = sem_open(sname, O_CREAT | O_EXCL, 0700, initial_value);

    //
    // This can happen if a semaphore of that name is already in use,
    // for example by another SDR program running on the same machine
    //
    if (sem == SEM_FAILED && errno == EEXIST) { continue; }

    break;
  }

  if (sem == SEM_FAILED) {
    t_perror("NewProtocol:SemOpen");
    exit (-1);
  }

  // we can unlink the semaphore NOW. It will remain functional
  // until sem_close() has been called by all threads using that
  // semaphore.
  sem_unlink(sname);
  return sem;
} 
#endif
